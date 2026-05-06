"""
Adaptive-trigger test harness for ESP32-BLE-CompositeHID's DualSense emulation.

The script connects to an ESP32 acting as a paired DualSense controller, sends
HID output reports (the same way a PS5 game does) covering every adaptive-
trigger subtype the firmware decodes, then reads the ESP32's debug serial
echo to verify it parsed each report correctly. Pass/fail per case; non-zero
exit on any failure.

Prerequisites
-------------
1. ESP32 flashed with examples/dualsenseExamples/Dualsense_Edge_Controller and
   already paired to this PC as a Sony DualSense (Settings -> Devices ->
   Wireless Controller). Pairing is a manual prereq -- not automated here.
2. No other process holding the controller (close Steam Input / DS4Windows /
   Game Bar) and no serial monitor holding the COM port (close Arduino IDE /
   PuTTY).
3. Python deps:  pip install pyserial pydualsense

Usage
-----
    python scripts/test-adaptive-triggers.py [--port COM3] [--baud 115200]
                                             [--timeout 2.0] [--filter NAME]
                                             [--verbose]

Notes on protocol coverage
--------------------------
pydualsense's high-level TriggerModes enum doesn't expose every byte we need
(no Off=0x05, no Weapon=0x25, no Vibration=0x26 — see DualsenseGamepadDevice.h
DsTriggerMode), and its forces[] array only covers 7 of the 10 trigger param
bytes. So we use pydualsense to find and open the HID device, but build the
78-byte BLE output report (report ID 0x31) ourselves and write it through the
underlying hid handle. Layout mirrors DualsenseGamepadOutputReportData in the
firmware.
"""

import argparse
import queue
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Tuple

try:
    import serial
except ImportError:
    sys.exit("Missing dependency: pip install pyserial")

try:
    from pydualsense import pydualsense
except ImportError:
    sys.exit("Missing dependency: pip install pydualsense")


# ----------------------------------------------------------------------------
# Protocol constants (from DualsenseGamepadDevice.h / DualsenseDescriptors.h)
# ----------------------------------------------------------------------------
DS_OUTPUT_REPORT_ID = 0x31
DS_OUTPUT_TAG = 0x10
DS_OUTPUT_REPORT_SIZE = 78  # full BLE wire size including report ID and CRC
PS_OUTPUT_CRC32_SEED = 0xA2

FLAG0_RIGHT_TRIGGER_EFFECT = 1 << 2
FLAG0_LEFT_TRIGGER_EFFECT = 1 << 3

MODE_OFF = 0x05
MODE_FEEDBACK = 0x21
MODE_WEAPON = 0x25
MODE_VIBRATION = 0x26

DS_VIDS_PIDS = [
    (0x054C, 0x0DF2),  # DualSense Edge (this firmware's default)
    (0x054C, 0x0CE6),  # DualSense (standard)
]


# ----------------------------------------------------------------------------
# CRC32 (matches DualsenseGamepadDevice::crc32_le — reflected, init 0xFFFFFFFF)
# ----------------------------------------------------------------------------
_CRC32_POLY_REFLECTED = 0xEDB88320
_CRC32_TABLE = []
for _b in range(256):
    _c = _b
    for _ in range(8):
        _c = (_c >> 1) ^ _CRC32_POLY_REFLECTED if (_c & 1) else (_c >> 1)
    _CRC32_TABLE.append(_c)


def crc32_le(crc: int, data: bytes) -> int:
    for byte in data:
        crc = (crc >> 8) ^ _CRC32_TABLE[(crc ^ byte) & 0xFF]
    return crc & 0xFFFFFFFF


# ----------------------------------------------------------------------------
# Output-report builder
# ----------------------------------------------------------------------------
def build_output_report(
    *,
    valid_flag0: int = 0,
    right_mode: int = 0,
    right_params: bytes = b"\x00" * 10,
    left_mode: int = 0,
    left_params: bytes = b"\x00" * 10,
    seq_tag: int = 0x10,
) -> bytes:
    """Build a 78-byte BLE DualSense output report (report ID 0x31).

    Layout matches DualsenseGamepadOutputReportData::load() in the firmware:
        byte  0: report_id (0x31)
        byte  1: seq_tag
        byte  2: tag (0x10 = DS_OUTPUT_TAG)
        bytes 3-49: 47-byte common section
        bytes 50-73: reserved (zeros)
        bytes 74-77: CRC32 (little-endian)
    """
    if len(right_params) != 10 or len(left_params) != 10:
        raise ValueError("trigger params must be exactly 10 bytes")

    r = bytearray(DS_OUTPUT_REPORT_SIZE)
    r[0] = DS_OUTPUT_REPORT_ID
    r[1] = seq_tag
    r[2] = DS_OUTPUT_TAG

    # common[0..1] = valid_flag0/1; common[2..3] = motor right/left;
    # common[4..9] = audio fields; common[10..20] = right trigger;
    # common[21..31] = left trigger; rest left zero.
    r[3] = valid_flag0
    r[13] = right_mode
    r[14:24] = right_params
    r[24] = left_mode
    r[25:35] = left_params

    # CRC over [seed, report_bytes 0..73]
    crc = crc32_le(0xFFFFFFFF, bytes([PS_OUTPUT_CRC32_SEED]))
    crc = (~crc32_le(crc, bytes(r[0:74]))) & 0xFFFFFFFF
    r[74] = crc & 0xFF
    r[75] = (crc >> 8) & 0xFF
    r[76] = (crc >> 16) & 0xFF
    r[77] = (crc >> 24) & 0xFF
    return bytes(r)


# ----------------------------------------------------------------------------
# Adaptive-trigger payload encoders (mirror firmware's classifySubtype helpers)
# ----------------------------------------------------------------------------
def _pack_strengths(strengths: List[int]) -> bytes:
    """Pack 10 strength nibbles (0-7 each, 3 bits per position) into 4 bytes.

    Position i lives at bit offset (i*3) within a 30-bit little-endian field.
    Returns the 4-byte field (params[2..5]).
    """
    if len(strengths) != 10:
        raise ValueError("expected 10 strengths")
    packed = 0
    for i, s in enumerate(strengths):
        if not 0 <= s <= 7:
            raise ValueError(f"strength {s} out of range 0..7")
        packed |= (s & 0x7) << (i * 3)
    return packed.to_bytes(4, "little")


def encode_off() -> bytes:
    return b"\x00" * 10


def encode_feedback(start: int, strength: int) -> bytes:
    """Uniform-strength feedback from `start` through position 9.

    `strength` is the user-facing value 1..8 (DSX scale). The wire uses the
    internal 0..7 scale (= DSX value - 1).
    """
    if not 0 <= start <= 9:
        raise ValueError("start must be 0..9")
    if not 1 <= strength <= 8:
        raise ValueError("strength must be 1..8")
    mask = ((1 << 10) - 1) & ~((1 << start) - 1)  # contiguous bits start..9
    strengths = [(strength - 1) if (mask >> i) & 1 else 0 for i in range(10)]
    params = bytearray(10)
    params[0:2] = mask.to_bytes(2, "little")
    params[2:6] = _pack_strengths(strengths)
    return bytes(params)


def encode_slope(start: int, end: int, start_strength: int, end_strength: int) -> bytes:
    """Slope feedback: strength ramps from start_strength at `start` to end_strength at `end`,
    then holds end_strength for positions end+1..9.

    The firmware classifies a Feedback (0x21) report as Slope only when the position mask is
    contiguous from `start` all the way to position 9 (bit 9 must be set). The `end` parameter
    controls where the ramp finishes — not where the mask ends.
    """
    if not 0 <= start < end <= 9:
        raise ValueError("require 0 <= start < end <= 9")
    if not 1 <= start_strength <= 8 or not 1 <= end_strength <= 8:
        raise ValueError("strengths must be 1..8")
    # Mask covers start..9 (contiguous to position 9 — firmware requirement for slope)
    mask = ((1 << 10) - 1) & ~((1 << start) - 1)
    span = end - start
    strengths = [0] * 10
    for i in range(start, 10):
        if i <= end:
            t = (i - start) / span
            v = round(start_strength + t * (end_strength - start_strength))
        else:
            v = end_strength  # hold end_strength from end+1 to 9
        strengths[i] = max(1, min(8, v)) - 1
    params = bytearray(10)
    params[0:2] = mask.to_bytes(2, "little")
    params[2:6] = _pack_strengths(strengths)
    return bytes(params)


def encode_multipos_feedback(per_position: List[int]) -> bytes:
    """Multi-position feedback: arbitrary subset of positions, each its own strength.

    `per_position` is a list of 10 user-facing strengths (0 = inactive, 1..8 = active).
    The mask must not be contiguous-from-start, otherwise the firmware will
    classify it as plain Feedback.
    """
    if len(per_position) != 10:
        raise ValueError("expected 10 strengths")
    mask = 0
    wire_strengths = [0] * 10
    for i, s in enumerate(per_position):
        if s:
            if not 1 <= s <= 8:
                raise ValueError(f"strength {s} at pos {i} out of range")
            mask |= 1 << i
            wire_strengths[i] = s - 1
    params = bytearray(10)
    params[0:2] = mask.to_bytes(2, "little")
    params[2:6] = _pack_strengths(wire_strengths)
    return bytes(params)


def encode_weapon(start: int, end: int, strength: int) -> bytes:
    """Weapon (gun-trigger) effect: resistance kicks in between `start` and `end`."""
    if not 0 <= start < end <= 9:
        raise ValueError("require 0 <= start < end <= 9")
    if not 1 <= strength <= 8:
        raise ValueError("strength must be 1..8")
    mask = (1 << start) | (1 << end)
    params = bytearray(10)
    params[0:2] = mask.to_bytes(2, "little")
    params[2] = strength - 1
    return bytes(params)


def encode_vibration(start: int, amplitude: int, frequency_hz: int) -> bytes:
    """Single-region vibration from `start` to position 9 at uniform amplitude.

    `amplitude` 1..8 (DSX), `frequency_hz` 1..255 (must be > 0 to register as
    Vibration; 0 would classify as Multi-Position Vibration with empty mask).
    """
    if not 0 <= start <= 9:
        raise ValueError("start must be 0..9")
    if not 1 <= amplitude <= 8:
        raise ValueError("amplitude must be 1..8")
    if not 1 <= frequency_hz <= 255:
        raise ValueError("frequency must be 1..255")
    mask = ((1 << 10) - 1) & ~((1 << start) - 1)
    amps = [(amplitude - 1) if (mask >> i) & 1 else 0 for i in range(10)]
    params = bytearray(10)
    params[0:2] = mask.to_bytes(2, "little")
    params[2:6] = _pack_strengths(amps)
    # params[6..8] zero -> distinguishes from Multi-Position Vibration
    params[9] = frequency_hz
    return bytes(params)


def encode_multipos_vibration(per_position_amp: List[int], frequency_hz: int) -> bytes:
    """Multi-position vibration: per-position amplitudes, single global frequency."""
    if len(per_position_amp) != 10:
        raise ValueError("expected 10 amplitudes")
    if not 1 <= frequency_hz <= 255:
        raise ValueError("frequency must be 1..255")
    mask = 0
    wire_amps = [0] * 10
    for i, a in enumerate(per_position_amp):
        if a:
            if not 1 <= a <= 8:
                raise ValueError(f"amp {a} at pos {i} out of range")
            mask |= 1 << i
            wire_amps[i] = a - 1
    params = bytearray(10)
    params[0:2] = mask.to_bytes(2, "little")
    params[2:6] = _pack_strengths(wire_amps)
    # params[8] = freq, params[9] = 0 -> classified as Multi-Position Vibration
    params[8] = frequency_hz
    return bytes(params)


# ----------------------------------------------------------------------------
# Test cases
# ----------------------------------------------------------------------------
@dataclass
class TestCase:
    name: str
    side: str  # "L" or "R"
    mode: int  # wire mode byte (0x05/0x21/0x25/0x26)
    params: bytes  # 10 bytes
    # Sketch label and expected fields. The sketch prints "L2=..." or " R2=..."
    # depending on side. We always strip whitespace before parsing.
    expected_label: str = field(init=False)
    expected_kind: str = ""  # "off", "feedback", "slope", "multipos", "weapon", "vibration", "multivib"
    expected_fields: Dict[str, object] = field(default_factory=dict)

    def __post_init__(self):
        self.expected_label = "L2" if self.side == "L" else "R2"


def build_test_cases() -> List[TestCase]:
    cases: List[TestCase] = []

    # OFF
    for side in ("L", "R"):
        cases.append(TestCase(
            name=f"off_{side}",
            side=side,
            mode=MODE_OFF,
            params=encode_off(),
            expected_kind="off",
            expected_fields={},
        ))

    # FEEDBACK (uniform strength, contiguous from start)
    cases.append(TestCase(
        name="feedback_L_start2_str5",
        side="L",
        mode=MODE_FEEDBACK,
        params=encode_feedback(start=2, strength=5),
        expected_kind="feedback",
        expected_fields={"start": 2, "strength": 5, "mask": 0x3FC},
    ))
    cases.append(TestCase(
        name="feedback_R_start6_str8",
        side="R",
        mode=MODE_FEEDBACK,
        params=encode_feedback(start=6, strength=8),
        expected_kind="feedback",
        expected_fields={"start": 6, "strength": 8, "mask": 0x3C0},
    ))

    # SLOPE FEEDBACK
    # Mask must cover start..9 for the firmware to classify as slope (not multipos).
    # end=7 means the ramp finishes at pos 7; positions 8-9 hold end_strength=7.
    # Firmware's end_position detection scans for the last strength change, yielding 7.
    cases.append(TestCase(
        name="slope_L_1to7_str3to7",
        side="L",
        mode=MODE_FEEDBACK,
        params=encode_slope(start=1, end=7, start_strength=3, end_strength=7),
        expected_kind="slope",
        expected_fields={"start": 1, "end": 7, "startStr": 3, "endStr": 7, "mask": 0x3FE},
    ))

    # MULTI-POSITION FEEDBACK (non-contiguous mask)
    multipos_strengths = [0, 4, 0, 6, 0, 0, 8, 0, 2, 0]
    cases.append(TestCase(
        name="multipos_R_alternating",
        side="R",
        mode=MODE_FEEDBACK,
        params=encode_multipos_feedback(multipos_strengths),
        expected_kind="multipos",
        expected_fields={
            "mask": 0x14A,  # bits 1,3,6,8
            "strengths": multipos_strengths,
        },
    ))

    # WEAPON
    cases.append(TestCase(
        name="weapon_L_2to6_str4",
        side="L",
        mode=MODE_WEAPON,
        params=encode_weapon(start=2, end=6, strength=4),
        expected_kind="weapon",
        expected_fields={"start": 2, "end": 6, "strength": 4, "mask": (1 << 2) | (1 << 6)},
    ))
    cases.append(TestCase(
        name="weapon_R_3to9_str7",
        side="R",
        mode=MODE_WEAPON,
        params=encode_weapon(start=3, end=9, strength=7),
        expected_kind="weapon",
        expected_fields={"start": 3, "end": 9, "strength": 7, "mask": (1 << 3) | (1 << 9)},
    ))

    # VIBRATION (single-region)
    cases.append(TestCase(
        name="vibration_L_start0_amp3_201Hz",
        side="L",
        mode=MODE_VIBRATION,
        params=encode_vibration(start=0, amplitude=3, frequency_hz=201),
        expected_kind="vibration",
        expected_fields={"start": 0, "amp": 3, "freq": 201, "mask": 0x3FF},
    ))
    cases.append(TestCase(
        name="vibration_R_start4_amp6_60Hz",
        side="R",
        mode=MODE_VIBRATION,
        params=encode_vibration(start=4, amplitude=6, frequency_hz=60),
        expected_kind="vibration",
        expected_fields={"start": 4, "amp": 6, "freq": 60, "mask": 0x3F0},
    ))

    # MULTI-POSITION VIBRATION
    multivib_amps = [5, 0, 5, 0, 5, 0, 5, 0, 5, 0]
    cases.append(TestCase(
        name="multivib_L_every_other_120Hz",
        side="L",
        mode=MODE_VIBRATION,
        params=encode_multipos_vibration(multivib_amps, frequency_hz=120),
        expected_kind="multivib",
        expected_fields={"freq": 120, "amps": multivib_amps},
    ))

    return cases


# ----------------------------------------------------------------------------
# Serial reader (background daemon)
# ----------------------------------------------------------------------------
class SerialReader(threading.Thread):
    def __init__(self, port: str, baud: int):
        super().__init__(daemon=True)
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.lines: "queue.Queue[Tuple[float, str]]" = queue.Queue()
        self._stop = threading.Event()

    def run(self) -> None:
        buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = self.ser.read(256)
            except serial.SerialException as e:
                self.lines.put((time.monotonic(), f"<<serial error: {e}>>"))
                return
            if not chunk:
                continue
            buf.extend(chunk)
            while b"\n" in buf:
                line, _, buf = buf.partition(b"\n")
                text = line.decode("utf-8", errors="replace").rstrip("\r")
                self.lines.put((time.monotonic(), text))

    def drain(self) -> None:
        while not self.lines.empty():
            try:
                self.lines.get_nowait()
            except queue.Empty:
                break

    def wait_for(self, predicate: Callable[[str], bool], timeout: float) -> Optional[str]:
        deadline = time.monotonic() + timeout
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                return None
            try:
                _, line = self.lines.get(timeout=remaining)
            except queue.Empty:
                return None
            if predicate(line):
                return line

    def close(self) -> None:
        self._stop.set()
        try:
            self.ser.close()
        except Exception:
            pass


# ----------------------------------------------------------------------------
# Serial echo parsing
# ----------------------------------------------------------------------------
# The sketch's formatTriggerEffect() emits one of:
#   off
#   feedback(start=N,strength=N,mask=0xHEX)
#   slope(start=N,end=N,startStr=N,endStr=N,mask=0xHEX)
#   multipos(mask=0xHEX,strengths=[a,b,c,d,e,f,g,h,i,j])
#   weapon(start=N,end=N,strength=N,mask=0xHEX)
#   vibration(start=N,amp=N,freq=NHz,mask=0xHEX)
#   multivib(freq=NHz,amps=[a,b,c,d,e,f,g,h,i,j])
#   unknown(0xHEX,params=[...])
_SEGMENT_RE = re.compile(r"(L2|R2)=(off|[a-z]+\([^)]*\))")


def _parse_kv_ints(body: str) -> Dict[str, int]:
    """Parse "a=1,b=2,mask=0xff" into a dict of ints."""
    out: Dict[str, int] = {}
    for kv in body.split(","):
        if "=" not in kv:
            continue
        k, v = kv.split("=", 1)
        v = v.strip()
        if v.endswith("Hz"):
            v = v[:-2]
        out[k.strip()] = int(v, 0)  # auto-detects 0x prefix
    return out


def _parse_array(body: str) -> List[int]:
    inside = body.strip()
    if inside.startswith("[") and inside.endswith("]"):
        inside = inside[1:-1]
    return [int(x.strip(), 0) for x in inside.split(",") if x.strip()]


def parse_trigger_segment(line: str, label: str) -> Optional[Dict[str, object]]:
    """Find the L2=... or R2=... segment in `line` and return the parsed effect.

    Returns a dict like {"kind": "feedback", "start": 2, ...} or None if not found.
    """
    for m in _SEGMENT_RE.finditer(line):
        if m.group(1) != label:
            continue
        effect = m.group(2)
        if effect == "off":
            return {"kind": "off"}
        kind, _, rest = effect.partition("(")
        rest = rest.rstrip(")")
        if kind in ("feedback", "slope", "weapon", "vibration"):
            d: Dict[str, object] = _parse_kv_ints(rest)  # type: ignore[assignment]
            d["kind"] = kind
            return d
        if kind == "multipos":
            # multipos(mask=0xHEX,strengths=[...])
            mask_part, _, list_part = rest.partition(",strengths=")
            return {
                "kind": "multipos",
                "mask": int(mask_part.split("=", 1)[1], 0),
                "strengths": _parse_array(list_part),
            }
        if kind == "multivib":
            # multivib(freq=NHz,amps=[...])
            freq_part, _, list_part = rest.partition(",amps=")
            freq_str = freq_part.split("=", 1)[1].rstrip("Hz")
            return {
                "kind": "multivib",
                "freq": int(freq_str, 0),
                "amps": _parse_array(list_part),
            }
        if kind == "unknown":
            return {"kind": "unknown", "raw": rest}
    return None


# ----------------------------------------------------------------------------
# Test runner
# ----------------------------------------------------------------------------
def open_controller() -> pydualsense:
    ds = pydualsense()
    # Newer pydualsense accepts a vid_pid kwarg; older versions don't.
    try:
        ds.init(vid_pid=DS_VIDS_PIDS)
    except TypeError:
        # Older pydualsense -- let it search its own default list.
        ds.init()
    # pydualsense's report_thread continuously sends output reports with its own
    # (zero-mode) trigger state, which would overwrite our test reports. Stop it.
    ds.ds_thread = False
    ds.report_thread.join(timeout=2.0)
    return ds


def hid_write(ds: pydualsense, report: bytes) -> int:
    """Write a raw output report through pydualsense's underlying hid handle."""
    dev = getattr(ds, "device", None)
    if dev is None:
        raise RuntimeError("pydualsense did not expose an open hid device")
    n = dev.write(bytes(report))
    if n is not None and n < 0:
        raise RuntimeError(f"hid write failed: {dev.error()!r}")
    return n


def run_case(
    case: TestCase,
    ds: pydualsense,
    reader: SerialReader,
    seq_tag_iter,
    timeout: float,
    verbose: bool,
) -> Tuple[bool, str]:
    if case.side == "L":
        flag = FLAG0_LEFT_TRIGGER_EFFECT
        report = build_output_report(
            valid_flag0=flag,
            left_mode=case.mode,
            left_params=case.params,
            seq_tag=next(seq_tag_iter),
        )
    else:
        flag = FLAG0_RIGHT_TRIGGER_EFFECT
        report = build_output_report(
            valid_flag0=flag,
            right_mode=case.mode,
            right_params=case.params,
            seq_tag=next(seq_tag_iter),
        )

    reader.drain()
    hid_write(ds, report)

    line = reader.wait_for(lambda l: f"{case.expected_label}=" in l, timeout / 2)
    if line is None:
        # BLE GATT Write-Without-Response can silently drop reports; retry once.
        hid_write(ds, report)
        line = reader.wait_for(lambda l: f"{case.expected_label}=" in l, timeout / 2)
    if line is None:
        return False, "no serial echo within timeout (tried twice)"
    if verbose:
        print(f"    serial: {line.strip()}")

    parsed = parse_trigger_segment(line, case.expected_label)
    if parsed is None:
        return False, f"could not parse {case.expected_label} segment from: {line.strip()!r}"

    if parsed.get("kind") != case.expected_kind:
        return False, (
            f"kind mismatch: expected {case.expected_kind!r}, got {parsed.get('kind')!r} "
            f"(segment: {line.strip()!r})"
        )

    for k, want in case.expected_fields.items():
        got = parsed.get(k)
        if got != want:
            return False, f"field {k!r}: expected {want!r}, got {got!r} (segment: {line.strip()!r})"

    return True, ""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", default="COM3", help="serial port the ESP32 is on (default: COM3)")
    ap.add_argument("--baud", type=int, default=115200, help="serial baud (default: 115200)")
    ap.add_argument("--timeout", type=float, default=2.0, help="seconds to wait per case (default: 2.0)")
    ap.add_argument("--filter", default=None, help="only run cases whose name contains this substring")
    ap.add_argument("--inter-case-delay", type=float, default=0.15,
                    help="seconds to sleep between cases (default: 0.15)")
    ap.add_argument("--verbose", action="store_true", help="print the matched serial line for every case")
    args = ap.parse_args()

    cases = build_test_cases()
    if args.filter:
        cases = [c for c in cases if args.filter in c.name]
        if not cases:
            print(f"No cases match filter {args.filter!r}", file=sys.stderr)
            return 2

    print(f"Opening serial {args.port} @ {args.baud}...")
    try:
        reader = SerialReader(args.port, args.baud)
    except serial.SerialException as e:
        print(f"Could not open {args.port}: {e}", file=sys.stderr)
        return 2
    reader.start()
    # Give the ESP32 a moment if the serial open caused a reset.
    time.sleep(0.5)
    reader.drain()

    print("Opening DualSense via pydualsense...")
    try:
        ds = open_controller()
    except Exception as e:
        reader.close()
        print(
            f"Could not open the controller: {e}\n"
            "Verify the ESP32 is paired (Settings -> Devices -> Wireless Controller),\n"
            "and that no other process (Steam Input, DS4Windows, Game Bar) holds it.",
            file=sys.stderr,
        )
        return 2

    # pydualsense's thread may have fired a final report right before we stopped it.
    # Wait for the ESP32 to echo it, then drain so it doesn't pollute the first test.
    time.sleep(0.8)
    reader.drain()

    # Warmup write: send a no-op report to prime the BLE GATT write channel.
    # The first write after connection is sometimes silently dropped by the BLE stack.
    noop = build_output_report()  # no flags set -- ESP32 won't echo this
    hid_write(ds, noop)
    time.sleep(0.2)
    reader.drain()

    def _seq_tags():
        s = 1
        while True:
            yield (s & 0xF) << 4
            s += 1

    seq_tag_iter = _seq_tags()

    passed = 0
    failed = 0
    failures: List[Tuple[str, str]] = []
    try:
        for c in cases:
            ok, why = run_case(c, ds, reader, seq_tag_iter, args.timeout, args.verbose)
            if ok:
                passed += 1
                print(f"[PASS] {c.name}")
            else:
                failed += 1
                failures.append((c.name, why))
                print(f"[FAIL] {c.name} -- {why}")
            time.sleep(args.inter_case_delay)
    finally:
        try:
            ds.close()
        except Exception:
            pass
        reader.close()

    total = passed + failed
    print()
    print(f"Summary: {passed} passed, {failed} failed ({total} total)")
    if failures:
        print("Failures:")
        for name, why in failures:
            print(f"  {name}: {why}")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
