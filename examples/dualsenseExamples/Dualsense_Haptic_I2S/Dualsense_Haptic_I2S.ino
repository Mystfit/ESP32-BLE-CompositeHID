/*
 * Dualsense_Haptic_I2S
 *
 * Worked example: receive DualSense haptic-audio frames over BLE and stream
 * them out an ESP32 I2S peripheral so an external DAC / class-D amp / VCA
 * driver can shake an actuator (LRA, voice coil, small speaker).
 *
 * Subscribes to DualsenseGamepadDevice::onHapticAudioReceived. Each fire
 * delivers 12 stereo 8-bit-signed PCM frames (24 bytes) extracted from the
 * 0x31 output report's reserved-region window. The callback runs on the
 * NimBLE host stack, so it copies samples into a ring buffer and exits
 * quickly; a separate task pulls from the ring buffer and writes to I2S.
 *
 * Format conversion: ESP32 I2S doesn't natively output 8-bit samples, so
 * we widen each int8_t to int16_t (<< 8) when copying. The output is
 * 16-bit signed stereo at I2S_SAMPLE_RATE_HZ — tune that to match your
 * host's haptic write rate × 12 frames-per-packet.
 *
 * Driving haptic audio into this firmware:
 *   - Linux: scripts/ds-haptic-probe.py with --pattern sine.
 *   - Windows / DSX: NOT supported; DSX's haptic UI is USB-only and the
 *     Windows BT-Classic-only audio path doesn't reach BLE peripherals.
 *     See the long comment in Dualsense_Edge_Controller.ino.
 *
 * Wiring (defaults; edit the pin defines for your board):
 *   I2S_BCLK_PIN  -> DAC bit clock
 *   I2S_LRCLK_PIN -> DAC word/LR clock
 *   I2S_DOUT_PIN  -> DAC data in
 *   GND/3V3 per your DAC's spec.
 */

#include <Arduino.h>
#include <BleConnectionStatus.h>
#include <BleCompositeHID.h>
#include <DualsenseGamepadDevice.h>
#include <driver/i2s.h>

#include "ArduinoDefines.h"

#define CONFIG_BT_NIMBLE_EXT_ADV 1

// --- I2S configuration -------------------------------------------------

#ifndef I2S_BCLK_PIN
#define I2S_BCLK_PIN   26
#endif
#ifndef I2S_LRCLK_PIN
#define I2S_LRCLK_PIN  25
#endif
#ifndef I2S_DOUT_PIN
#define I2S_DOUT_PIN   22
#endif
#ifndef I2S_PORT
#define I2S_PORT       I2S_NUM_0
#endif

// Output sample rate. Match this to the host's effective output rate
// (frames-per-packet × packets-per-second). Common cases:
//   - Linux ds-haptic-probe.py / older 0x32 audio path: ~1500 Hz fits
//     12 frames/packet × ~100 Hz reporting.
//   - Unreal-Dualsense plugin (0x36 audio path): the plugin's submix
//     listener resamples to 3000 Hz internally and emits 32 frames/packet
//     in two halves, so the effective rate is 3000 Hz.
// Pick the higher rate when uncertain; the ring buffer absorbs stalls.
#ifndef I2S_SAMPLE_RATE_HZ
#define I2S_SAMPLE_RATE_HZ  3000
#endif

// --- Ring buffer (single producer = BLE cb, single consumer = I2S task) -

// Holds widened 16-bit stereo frames. 2 KiB total = 512 stereo frames =
// ~341 ms of audio at 1.5 kHz, plenty for short host hiccups.
static const size_t HAPTIC_RING_FRAMES = 512;
static int16_t      g_ring[HAPTIC_RING_FRAMES * 2];   // L R L R ...
static volatile size_t g_ring_head = 0;               // producer writes here
static volatile size_t g_ring_tail = 0;               // consumer reads here
static volatile uint32_t g_overflow_frames = 0;
static volatile uint32_t g_pushed_frames   = 0;

int ledPin = LED_BUILTIN; // LED connected to digital pin 8

// Latest signed sample value the BLE callback pushed. The loop reads it at
// ~50 Hz and drives the LED based on its sign (positive half-cycle = on,
// negative = off, silence = off). For a slow sine (e.g. --sine-freq 5)
// this gives a clearly visible blink at the sine's frequency. For sines
// faster than ~25 Hz the LED update aliases and won't show the cycle —
// that's a fundamental limit of a 50 Hz visual sampler.
static volatile int16_t g_led_last = 0;

static inline size_t ring_used_frames()
{
    size_t head = g_ring_head;
    size_t tail = g_ring_tail;
    return (head - tail) & (HAPTIC_RING_FRAMES - 1);
}

static inline size_t ring_free_frames()
{
    return HAPTIC_RING_FRAMES - 1 - ring_used_frames();
}

// HAPTIC_RING_FRAMES must be a power of two for the bitmask wrap above.
static_assert((HAPTIC_RING_FRAMES & (HAPTIC_RING_FRAMES - 1)) == 0,
              "HAPTIC_RING_FRAMES must be a power of two");

// --- BLE haptic-audio callback -----------------------------------------

static void OnHapticAudio(HapticAudioFrame frame)
{
    if (!frame.samples || frame.sampleCount == 0) return;

    size_t free_n = ring_free_frames();
    if (frame.sampleCount > free_n) {
        g_overflow_frames += (frame.sampleCount - free_n);
    }
    size_t to_push = (frame.sampleCount < free_n) ? frame.sampleCount : free_n;

    size_t head = g_ring_head;
    int16_t last_sample = 0;
    for (size_t i = 0; i < to_push; ++i) {
        int8_t  L8 = frame.samples[2 * i + 0];
        int8_t  R8 = frame.samples[2 * i + 1];
        int16_t L16 = (int16_t)((int16_t)L8 << 8);
        int16_t R16 = (int16_t)((int16_t)R8 << 8);
        g_ring[(head & (HAPTIC_RING_FRAMES - 1)) * 2 + 0] = L16;
        g_ring[(head & (HAPTIC_RING_FRAMES - 1)) * 2 + 1] = R16;
        ++head;
        last_sample = L16;
    }
    g_ring_head = head;
    g_pushed_frames += (uint32_t)to_push;
    if (to_push > 0) g_led_last = last_sample;
}

FunctionSlot<HapticAudioFrame> hapticSlot(OnHapticAudio);

// --- Output-report diagnostics -----------------------------------------
//
// Tally output-report IDs as they arrive so we can see at a glance which
// transport the host is using. The Unreal-Dualsense plugin (and other hosts)
// will send 0x02 USB-style reports if it classifies the device as USB, and
// only emit 0x32 audio-haptic writes when classified as Bluetooth. This
// makes it obvious whether haptic-audio packets are actually transiting BLE.
static volatile uint32_t g_reports_0x02   = 0;
static volatile uint32_t g_reports_0x31   = 0;
static volatile uint32_t g_reports_0x32   = 0;
static volatile uint32_t g_reports_0x36   = 0;
static volatile uint32_t g_reports_other  = 0;

static void OnOutputReport(DualsenseGamepadOutputReportData data)
{
    switch (data.report_id) {
        case 0x02: g_reports_0x02++;  break;
        case 0x31: g_reports_0x31++;  break;
        case 0x32: g_reports_0x32++;  break;
        case 0x36: g_reports_0x36++;  break;
        default:   g_reports_other++; break;
    }
}

FunctionSlot<DualsenseGamepadOutputReportData> outputSlot(OnOutputReport);

// --- I2S setup + drain --------------------------------------------------

static void setupI2S()
{
    i2s_config_t cfg = {};
    cfg.mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
    cfg.sample_rate          = I2S_SAMPLE_RATE_HZ;
    cfg.bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count        = 4;
    cfg.dma_buf_len          = 64;
    cfg.use_apll             = false;
    cfg.tx_desc_auto_clear   = true;

    i2s_pin_config_t pins = {};
    pins.bck_io_num   = I2S_BCLK_PIN;
    pins.ws_io_num    = I2S_LRCLK_PIN;
    pins.data_out_num = I2S_DOUT_PIN;
    pins.data_in_num  = I2S_PIN_NO_CHANGE;

    if (i2s_driver_install(I2S_PORT, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("i2s_driver_install failed");
        return;
    }
    i2s_set_pin(I2S_PORT, &pins);
    i2s_zero_dma_buffer(I2S_PORT);
    Serial.printf("I2S up: %d Hz, 16-bit stereo on BCLK=%d LRCLK=%d DOUT=%d\n",
                  I2S_SAMPLE_RATE_HZ, I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN);
}

static void drainRingToI2S()
{
    // Pull contiguous frames out of the ring and hand them to I2S in one
    // write per pass. Non-blocking from the perspective of the BLE callback
    // because we run from loop().
    size_t used = ring_used_frames();
    if (used == 0) return;

    size_t tail   = g_ring_tail;
    size_t span   = HAPTIC_RING_FRAMES - (tail & (HAPTIC_RING_FRAMES - 1));
    size_t to_pop = (used < span) ? used : span;
    if (to_pop == 0) return;

    size_t bytes_to_write = to_pop * 2 * sizeof(int16_t);
    size_t bytes_written  = 0;
    int16_t* src = &g_ring[(tail & (HAPTIC_RING_FRAMES - 1)) * 2];

    i2s_write(I2S_PORT, src, bytes_to_write, &bytes_written,
              pdMS_TO_TICKS(10));

    size_t frames_written = bytes_written / (2 * sizeof(int16_t));
    g_ring_tail = tail + frames_written;
}

// --- BLE setup + heartbeat input report --------------------------------

DualsenseGamepadDevice* dualsense = nullptr;
BleCompositeHID compositeHID("DS Haptic I2S", "Mystfit", 100);

static uint32_t last_stats_ms = 0;
static void emitStatsIfDue()
{
    uint32_t now = millis();
    if (now - last_stats_ms < 5000) return;
    last_stats_ms = now;
    Serial.printf("[stats] pushed=%u overflow=%u ring_used=%u | reports: 0x02=%u 0x31=%u 0x32=%u 0x36=%u other=%u\n",
                  (unsigned)g_pushed_frames,
                  (unsigned)g_overflow_frames,
                  (unsigned)ring_used_frames(),
                  (unsigned)g_reports_0x02,
                  (unsigned)g_reports_0x31,
                  (unsigned)g_reports_0x32,
                  (unsigned)g_reports_0x36,
                  (unsigned)g_reports_other);
}

// Sample the latest sample value at ~50 Hz and threshold it: positive
// half-cycle of the sine -> LED on, negative -> LED off, silence -> off.
// This visualizes the sine's *phase*, not its amplitude. Run the probe
// at a low frequency to see the blink:
//   sudo python3 scripts/ds-haptic-probe.py --pattern sine --sine-freq 5
// At 5 Hz you'll see a clean 5 Hz on/off pattern. At 200 Hz the 50 Hz
// LED sampler aliases and the LED looks roughly steady-on — that's
// physics, not a bug; you can't visualize 200 Hz with the human eye.
static uint32_t last_led_ms = 0;
static void updateLEDIfDue()
{
    uint32_t now = millis();
    if (now - last_led_ms < 20) return;
    last_led_ms = now;

    int16_t latest = g_led_last;
    digitalWrite(ledPin, latest > 0 ? HIGH : LOW);
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println();
    Serial.println("=== DualSense Haptic I2S ===");

    setupI2S();

    pinMode(ledPin, OUTPUT); // sets the digital pin as output

    DualsenseEdgeControllerDeviceConfiguration* config =
        new DualsenseEdgeControllerDeviceConfiguration();
    config->setAutoReport(true);
    config->setAutoDefer(false);

    dualsense = new DualsenseGamepadDevice(config);
    dualsense->onHapticAudioReceived.attach(hapticSlot);
    dualsense->onReceivedOutputReport.attach(outputSlot);

    compositeHID.addDevice(dualsense);
    compositeHID.begin(config->getIdealHostConfiguration());

    Serial.println("Advertising. Pair from a Linux host and feed haptics");
    Serial.println("with scripts/ds-haptic-probe.py --pattern sine.");

    // Wait for the host to bind, then announce calibration/firmware/pairing
    // feature reports. Hosts that key off these (e.g. the Unreal-Dualsense
    // plugin) treat a controller that never advertises them as not-yet-
    // initialized and will mark it disconnected even while BLE is up. Mirrors
    // Dualsense_Edge_Controller.ino's post-connect block.
    while (!compositeHID.isConnected()) {
        delay(100);
    }
    delay(280);

    dualsense->sendPairingInfoReport();
    dualsense->sendFirmInfoReport();
    dualsense->sendCalibrationReport();
    dualsense->resetInputs();
}

void loop()
{
    drainRingToI2S();
    updateLEDIfDue();
    emitStatsIfDue();

    if (compositeHID.isConnected()) {
        // Heartbeat input reports keep the host's HID-over-GATT bridge happy
        // and let it escalate to the full 0x31 output path. seq() bumps the
        // sequence counter and sends one report — no extra sendGamepadReport()
        // call is needed. ~50 Hz matches Dualsense_Edge_Controller.ino.
        dualsense->timestamp();
        dualsense->seq();
    }
    delay(20);
}
