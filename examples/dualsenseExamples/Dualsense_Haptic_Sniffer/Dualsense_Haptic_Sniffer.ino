/*
 * DualSense Haptic Sniffer
 *
 * Phase-1 diagnostic sketch for adding haptic-audio support to the
 * ESP32-BLE-CompositeHID DualSense emulation. This sketch does NOT yet
 * parse haptic audio. Its job is to capture raw 0x31 output reports as
 * they arrive from the host so we can locate the bytes that carry the
 * audio-rate PCM (if any) by inspection.
 *
 * The pre-existing BLE-REACHABLE HAPTIC PROXY note in the Edge example
 * states that "true VCA haptic audio (DSX 'BT Haptics' / 'Sound Waves')
 * is streamed over the Bluetooth Classic HID interrupt channel via an
 * undocumented Sony firmware path... it does not reach BLE HoGP
 * peripherals." This sketch is the empirical test of that claim,
 * re-evaluated in light of the SAxense PoC
 * (https://github.com/egormanga/SAxense), which feeds 8-bit PCM into a
 * real DualSense via HID output reports on Linux. If the BLE pipe DOES
 * carry audio-rate data under some host configuration, this sniffer
 * will surface it as a fast-mutating band of bytes outside the named
 * control fields.
 *
 * What it prints
 * --------------
 *  - One summary line per incoming output report:
 *      cnt=N len=L id=0x31 seq=0xXX tag=0xXX vf0=.. vf1=.. vf2=..
 *      mL=NN mR=NN ac=NN ac2=NN diff=K  map=[........|****....|...]
 *    The `map` field is a coarse XOR-vs-previous-report visualization,
 *    one cell per 8-byte block of the 47-byte common section and any
 *    trailing reserved region: '*' = at least one byte in that block
 *    changed since the previous report; '.' = unchanged. The named
 *    control fields cluster in the early blocks; haptic-audio (if it
 *    exists on this transport) would appear as persistent '*' in
 *    later blocks.
 *  - Every Nth report, a full hexdump of the raw buffer.
 *  - Marker lines on demand so captures can be labelled.
 *
 * Serial commands
 * ---------------
 *  r            Reset XOR baseline (clear "previous" buffer).
 *  f<n>         Set full-hex-dump cadence to every Nth report (0=off).
 *  m<text>      Print a marker line (e.g. "m switching DSX preset").
 *  ?            Help.
 *
 * Suggested capture protocol
 * --------------------------
 *  1. Pair the ESP32 to a host (Windows + DSX, or Linux).
 *  2. `m baseline-idle` — capture a few seconds with nothing playing.
 *  3. `m rumble-only`  — load a DSX rumble preset.
 *  4. `m trigger-only` — load an adaptive-trigger preset.
 *  5. `m haptic-audio` — load a DSX "BT Haptics" / "Sound Waves" preset.
 *  6. Compare the `map` column across markers. Audio-rate data, if it
 *     reaches us, will show as sustained '*' in blocks where the other
 *     captures show '.'.
 *
 * On Linux you can also write synthetic 0x31 reports directly to the
 * emulated controller's hidraw node (see the Phase-3 plan for a small
 * Python script). That gives a known-good ground truth for locating
 * the PCM offsets independent of host quirks.
 */

#include <BleConnectionStatus.h>
#include <BleCompositeHID.h>
#include <DualsenseGamepadDevice.h>
#include "ArduinoDefines.h"

#define CONFIG_BT_NIMBLE_EXT_ADV 1

// --- State -------------------------------------------------------------

static const size_t  PREV_BUF_CAP   = 547;   // descriptor max output size
static uint8_t       prev_buf[PREV_BUF_CAP] = { 0 };
static size_t        prev_len       = 0;
static uint32_t      report_count   = 0;
static uint32_t      changed_count  = 0;     // reports where diff>0
static uint32_t      last_stats_ms  = 0;
static bool          verbose        = false; // print every report when true
static uint32_t      full_dump_every_changed = 1; // dump on every changed report by default

DualsenseGamepadDevice* dualsense = nullptr;
BleCompositeHID compositeHID("DS Haptic Sniffer", "Mystfit", 100);

// --- Helpers -----------------------------------------------------------

static void printHexLine(const uint8_t* p, size_t len, size_t offset)
{
    Serial.printf("  %04u: ", (unsigned)offset);
    for (size_t i = 0; i < len; ++i) {
        Serial.printf("%02X", p[i]);
        Serial.print((i + 1) % 4 == 0 ? " " : "");
    }
    Serial.println();
}

static void hexDump(const uint8_t* p, size_t len)
{
    const size_t WIDTH = 16;
    for (size_t off = 0; off < len; off += WIDTH) {
        size_t row = (len - off < WIDTH) ? (len - off) : WIDTH;
        printHexLine(p + off, row, off);
    }
}

// One '*' or '.' per BLOCK bytes covering [start, end). Highlights regions
// that mutate between consecutive reports.
static void printDiffMap(const uint8_t* cur, size_t cur_len,
                         const uint8_t* prev, size_t prev_len_,
                         size_t start, size_t end, size_t block)
{
    Serial.print("[");
    for (size_t b = start; b < end; b += block) {
        bool changed = false;
        size_t b_end = b + block;
        if (b_end > end) b_end = end;
        for (size_t i = b; i < b_end; ++i) {
            uint8_t c = (i < cur_len)  ? cur[i]  : 0;
            uint8_t p = (i < prev_len_) ? prev[i] : 0;
            if (c != p) { changed = true; break; }
        }
        Serial.print(changed ? '*' : '.');
        // Visual separator every 8 cells (i.e. every 64 bytes when block=8).
        if (((b - start) / block + 1) % 8 == 0 && b + block < end) {
            Serial.print('|');
        }
    }
    Serial.print("]");
}

static size_t countByteDiffs(const uint8_t* cur, size_t cur_len,
                             const uint8_t* prev, size_t prev_len_)
{
    size_t n = (cur_len > prev_len_) ? cur_len : prev_len_;
    size_t d = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t c = (i < cur_len)  ? cur[i]  : 0;
        uint8_t p = (i < prev_len_) ? prev[i] : 0;
        if (c != p) ++d;
    }
    return d;
}

// --- Output-report slot ------------------------------------------------

static void printReportLine(const DualsenseGamepadOutputReportData& data,
                            const uint8_t* raw, size_t len, size_t diffs)
{
    Serial.printf("cnt=%u len=%u id=0x%02X seq=0x%02X tag=0x%02X "
                  "vf0=%02X vf1=%02X vf2=%02X "
                  "mL=%02X mR=%02X ac=%02X ac2=%02X "
                  "diff=%u co=%d  map=",
                  (unsigned)report_count, (unsigned)len,
                  data.report_id, data.seq_tag, data.tag,
                  data.valid_flag0, data.valid_flag1, data.valid_flag2,
                  data.motor_left, data.motor_right,
                  data.audio_control, data.audio_control2,
                  (unsigned)diffs, data.common_offset_used);
    printDiffMap(raw, len, prev_buf, prev_len, 0, len, 8);
    Serial.println();
}

static void OnOutputReport(DualsenseGamepadOutputReportData data)
{
    ++report_count;

    const uint8_t* raw = data.raw_data;
    size_t         len = data.raw_size;
    if (!raw || len == 0) {
        Serial.printf("cnt=%u (no raw buffer attached — load() did not stash it)\n",
                      (unsigned)report_count);
        return;
    }

    size_t diffs = countByteDiffs(raw, len, prev_buf, prev_len);
    bool   first = (prev_len == 0);
    if (diffs > 0) ++changed_count;

    // Print on every change, on the first report, or when the user has
    // enabled verbose mode. Idle heartbeat reports (diff=0) are suppressed
    // so the serial log stays readable at high report rates.
    bool should_print = verbose || first || (diffs > 0);

    if (should_print) {
        printReportLine(data, raw, len, diffs);
        if (full_dump_every_changed && (changed_count % full_dump_every_changed) == 0) {
            Serial.printf("--- hex (cnt=%u, changed=%u) ---\n",
                          (unsigned)report_count, (unsigned)changed_count);
            hexDump(raw, len);
        }
    }

    // Stash for next diff. raw_data is borrowed from the live BLE buffer
    // so we must copy out before the callback returns.
    size_t to_copy = (len < PREV_BUF_CAP) ? len : PREV_BUF_CAP;
    memcpy(prev_buf, raw, to_copy);
    prev_len = to_copy;
}

static void emitStatsIfDue()
{
    uint32_t now = millis();
    if (last_stats_ms == 0) { last_stats_ms = now; return; }
    if (now - last_stats_ms < 5000) return;
    Serial.printf("[stats] %u reports in last %u ms, %u with diff>0\n",
                  (unsigned)report_count, (unsigned)(now - last_stats_ms),
                  (unsigned)changed_count);
    last_stats_ms = now;
    report_count   = 0;
    changed_count  = 0;
}

FunctionSlot<DualsenseGamepadOutputReportData> outputSlot(OnOutputReport);

// --- Serial command handler --------------------------------------------

static void handleSerial()
{
    if (!Serial.available()) return;

    int c = Serial.read();
    if (c < 0) return;

    if (c == '\r' || c == '\n') return;

    if (c == '?') {
        Serial.println();
        Serial.println("commands:");
        Serial.println("  r          reset XOR baseline");
        Serial.println("  f<n>       hex dump every Nth CHANGED report (0=off, 1=every)");
        Serial.println("  v          toggle verbose mode (print every report, including diff=0)");
        Serial.println("  m<text>    print a marker line");
        Serial.println("  ?          this help");
        return;
    }

    if (c == 'v') {
        verbose = !verbose;
        Serial.printf("--- verbose=%s ---\n", verbose ? "on" : "off");
        return;
    }

    if (c == 'r') {
        memset(prev_buf, 0, sizeof(prev_buf));
        prev_len = 0;
        Serial.println("--- baseline reset ---");
        return;
    }

    if (c == 'f') {
        uint32_t n = (uint32_t)Serial.parseInt();
        full_dump_every_changed = n;
        Serial.printf("--- hex-dump cadence = every %u changed reports ---\n",
                      (unsigned)full_dump_every_changed);
        return;
    }

    if (c == 'm') {
        // Read until newline.
        String label = Serial.readStringUntil('\n');
        label.trim();
        Serial.print("===== MARKER: ");
        Serial.print(label);
        Serial.printf(" (cnt=%u) =====\n", (unsigned)report_count);
        return;
    }

    Serial.printf("unknown command '%c' (try '?')\n", c);
}

// --- Setup / loop ------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("=== DualSense Haptic Sniffer ===");
    Serial.println("Type '?' for commands.");

    DualsenseEdgeControllerDeviceConfiguration* config =
        new DualsenseEdgeControllerDeviceConfiguration();
    config->setAutoReport(true);
    config->setAutoDefer(false);

    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
    Serial.printf("VID=0x%04X PID=0x%04X\n", hostConfig.getVid(), hostConfig.getPid());

    dualsense = new DualsenseGamepadDevice(config);
    dualsense->onReceivedOutputReport.attach(outputSlot);

    compositeHID.addDevice(dualsense);
    compositeHID.begin(hostConfig);

    Serial.println("Advertising. Pair from the host now.");
}

void loop()
{
    handleSerial();
    emitStatsIfDue();

    if (compositeHID.isConnected()) {
        // Keep input pipe alive with neutral input reports; without these,
        // some hosts (Steam) won't escalate to the full 0x31 path.
        dualsense->timestamp();
        dualsense->seq();
        dualsense->sendGamepadReport();
    }
    delay(20);
}
