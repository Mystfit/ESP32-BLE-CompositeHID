#!/usr/bin/env python3
"""
ds-haptic-probe.py — Linux protocol-oracle probe for haptic-audio support.

Writes synthetic 78-byte 0x31 BT output reports to a paired emulated DualSense
(ESP32 flashed with examples/dualsenseExamples/Dualsense_Haptic_Sniffer) so we
can determine, independent of any host stack (Windows/DSX/Steam/etc.),
whether the BLE GATT output-report path carries arbitrary bytes through to
our firmware. Linux is used because /dev/hidraw lets userspace write raw HID
output reports; this is the same path SAxense uses against a real DualSense.

What the test answers
---------------------
Background: the existing project note in
  examples/dualsenseExamples/Dualsense_Edge_Controller/Dualsense_Edge_Controller.ino
states that VCA haptic audio rides Bluetooth Classic HID and "does not reach
BLE HoGP peripherals." A DSX-on-Windows capture confirmed: zero output-report
mutation reaches the firmware. That tells us about the *Windows host stack*,
not about the BLE pipe itself.

This script tests the pipe directly: it writes known patterns into a
configurable byte window of the 0x31 output report at a configurable rate.
The Dualsense_Haptic_Sniffer firmware on the ESP32 will print `diff>0` lines
and stats showing exactly which reports arrived and how many bytes mutated.
If the pipe carries the bytes, we see them — and we know the limit was the
host stack, not the transport.

Usage
-----
    sudo python3 ds-haptic-probe.py [--device /dev/hidrawN]
                                    [--rate-hz 50] [--duration 5]
                                    [--pattern counter|sine|constant]
                                    [--offset N] [--length L] [--value V]
                                    [--no-crc]

Pairing prereq: ESP32 flashed with the haptic sniffer sketch and paired via
bluetoothctl. Watch the ESP32 serial monitor (115200 baud) while running.

Notes
-----
* 0x31 BT output report layout (mirrors firmware DualsenseGamepadOutputReportData
  / Linux dualsense_output_report_bt):
    byte  0       : report_id  = 0x31
    byte  1       : seq_tag    (upper 4 bits = seq, lower 4 = tag)
    byte  2       : tag        = 0x10
    bytes 3..49   : 47-byte common section (motors, audio control, triggers,
                    timestamp, valid_flag2, lightbar, etc.)
    bytes 50..73  : 24-byte reserved region — the most plausible window for
                    haptic-audio bytes given the wire size; default probe
                    target.
    bytes 74..77  : CRC32 (LE) over the preceding 78 - 4 bytes, prepended
                    with the SEED byte 0xA2 (per Sony BT HID convention).
* For the byte window probe, the firmware does not currently validate CRC on
  output reports, so --no-crc is fine for transport-only testing. Compute
  CRC anyway by default to mirror what a real driver would send.
"""

import argparse
import glob
import math
import os
import re
import struct
import sys
import time
import zlib
from typing import Optional

DS_REPORT_ID_BT  = 0x31
DS_OUTPUT_TAG    = 0x10
DS_REPORT_LEN_BT = 78
DS_CRC_SEED      = 0xA2  # BT output-report CRC seed prepended to the buffer

DS_VID = 0x054C
DS_PID = 0x0CE6


# ---------------------------------------------------------------------------
# Device discovery
# ---------------------------------------------------------------------------

def _read_id(sysfs_path: str) -> Optional[str]:
    try:
        with open(sysfs_path, "r") as fh:
            return fh.read().strip()
    except OSError:
        return None


def list_hidraw_candidates() -> list:
    """Return [(/dev/hidrawN, HID_ID string, HID_NAME string), ...] for every
    hidraw node currently visible. Used both for auto-detect and for failure
    diagnostics so the user can see exactly what BlueZ exposed."""
    out = []
    for node in sorted(glob.glob("/sys/class/hidraw/hidraw*/device/uevent")):
        text = _read_id(node) or ""
        hid_id = ""
        hid_name = ""
        for line in text.splitlines():
            if line.startswith("HID_ID="):
                hid_id = line[len("HID_ID="):]
            elif line.startswith("HID_NAME="):
                hid_name = line[len("HID_NAME="):]
        name = node.split("/")[-3]  # hidrawN
        out.append((f"/dev/{name}", hid_id, hid_name))
    return out


def find_hidraw_for_dualsense() -> Optional[str]:
    """Return /dev/hidrawN for the first node whose modalias matches Sony DS."""
    for dev, hid_id, _ in list_hidraw_candidates():
        m = re.search(r"\w+:0*([0-9A-Fa-f]+):0*([0-9A-Fa-f]+)", hid_id)
        if not m:
            continue
        vid, pid = int(m.group(1), 16), int(m.group(2), 16)
        if vid == DS_VID and pid == DS_PID:
            return dev
    return None


# ---------------------------------------------------------------------------
# Report construction
# ---------------------------------------------------------------------------

def crc32_with_seed(buf: bytes, seed_byte: int) -> int:
    """DualSense BT CRC: zlib.crc32(seed || buf), little-endian."""
    return zlib.crc32(bytes([seed_byte]) + buf) & 0xFFFFFFFF


def build_report(payload_bytes: bytes,
                 offset: int,
                 seq: int,
                 with_crc: bool = True) -> bytes:
    """Construct a 78-byte 0x31 BT output report with `payload_bytes` written
    starting at `offset`. The byte at index 0 is the report ID, so `offset`
    is relative to the start of the report including the report-id byte.
    """
    if offset < 3:
        raise ValueError("offset must be >= 3 (after report_id/seq_tag/tag)")
    if offset + len(payload_bytes) > DS_REPORT_LEN_BT - (4 if with_crc else 0):
        raise ValueError("payload extends into CRC region")

    buf = bytearray(DS_REPORT_LEN_BT)
    buf[0] = DS_REPORT_ID_BT
    buf[1] = ((seq & 0x0F) << 4) | 0x00      # upper nibble seq, lower tag
    buf[2] = DS_OUTPUT_TAG
    buf[offset:offset + len(payload_bytes)] = payload_bytes
    if with_crc:
        crc = crc32_with_seed(bytes(buf[:DS_REPORT_LEN_BT - 4]), DS_CRC_SEED)
        struct.pack_into("<I", buf, DS_REPORT_LEN_BT - 4, crc)
    return bytes(buf)


# ---------------------------------------------------------------------------
# Pattern generators
# ---------------------------------------------------------------------------

def gen_counter(length: int, step: int) -> bytes:
    """One byte of counter, replicated to `length` bytes."""
    return bytes([(step & 0xFF)] * length)


def gen_sine(length: int, step: int, sample_rate_hz: float, freq_hz: float) -> bytes:
    """`length` 8-bit signed PCM samples of a sine at `freq_hz`, taken from a
    continuous oscillator advanced by `length` samples per call."""
    out = bytearray(length)
    base_phase = (step * length) / sample_rate_hz
    for i in range(length):
        t = base_phase + i / sample_rate_hz
        s = int(round(127 * math.sin(2 * math.pi * freq_hz * t)))
        out[i] = s & 0xFF
    return bytes(out)


def gen_constant(length: int, value: int) -> bytes:
    return bytes([value & 0xFF] * length)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", help="hidraw node (default: auto-detect Sony DS)")
    ap.add_argument("--rate-hz", type=float, default=50.0,
                    help="output reports per second (default: 50)")
    ap.add_argument("--duration", type=float, default=5.0,
                    help="seconds to run; 0 = run until Ctrl-C (default: 5)")
    ap.add_argument("--pattern", choices=("counter", "sine", "constant"),
                    default="counter",
                    help="payload pattern (default: counter — most diagnosable)")
    ap.add_argument("--offset", type=int, default=50,
                    help="byte offset within the 78-byte report (default: 50, "
                         "the start of the 24-byte reserved region)")
    ap.add_argument("--length", type=int, default=24,
                    help="payload length in bytes (default: 24)")
    ap.add_argument("--value", type=lambda s: int(s, 0), default=0xAA,
                    help="constant byte value (default: 0xAA)")
    ap.add_argument("--sine-freq", type=float, default=200.0,
                    help="sine pattern frequency in Hz (default: 200)")
    ap.add_argument("--sine-srate", type=float, default=3000.0,
                    help="sine sample rate in Hz (default: 3000, matches "
                         "the rate documented for DS haptic audio)")
    ap.add_argument("--no-crc", action="store_true",
                    help="omit CRC32 trailer (firmware doesn't validate output CRC)")
    ap.add_argument("--verbose", action="store_true",
                    help="print every report we write (slow, debugging only)")
    args = ap.parse_args()

    device = args.device or find_hidraw_for_dualsense()
    if not device:
        candidates = list_hidraw_candidates()
        if not candidates:
            sys.exit("no /dev/hidraw* nodes exist at all. BlueZ's HID-over-GATT "
                     "bridge probably didn't claim the controller. Try:\n"
                     "  sudo modprobe uhid\n"
                     "  bluetoothctl> disconnect <MAC>; connect <MAC>\n"
                     "Then re-run this script.")
        print("no hidraw node matched Sony DualSense (054C:0CE6).", file=sys.stderr)
        print("hidraw nodes currently visible:", file=sys.stderr)
        for dev, hid_id, hid_name in candidates:
            print(f"  {dev}  HID_ID={hid_id or '(none)'}  HID_NAME={hid_name or '(none)'}",
                  file=sys.stderr)
        sys.exit("if one of these is your ESP32, re-run with --device <path>; "
                 "BlueZ on some kernel versions doesn't propagate the descriptor "
                 "VID/PID into the hidraw uevent for HoGP devices.")
    print(f"using {device}")

    interval = 1.0 / args.rate_hz
    end_at = (time.monotonic() + args.duration) if args.duration > 0 else None

    try:
        fd = os.open(device, os.O_WRONLY)
    except OSError as e:
        sys.exit(f"open({device}): {e} (try sudo)")

    print(f"writing {args.pattern} pattern at offset={args.offset} "
          f"length={args.length} rate={args.rate_hz} Hz "
          f"crc={'no' if args.no_crc else 'yes'}")
    print("watch the ESP32 serial monitor; sniffer's [stats] line should "
          "report diff>0 for these writes")

    sent = 0
    next_t = time.monotonic()
    try:
        while True:
            if end_at and time.monotonic() >= end_at:
                break

            if args.pattern == "counter":
                payload = gen_counter(args.length, sent)
            elif args.pattern == "sine":
                payload = gen_sine(args.length, sent,
                                   args.sine_srate, args.sine_freq)
            else:  # constant
                payload = gen_constant(args.length, args.value)

            report = build_report(payload, args.offset, sent,
                                  with_crc=not args.no_crc)
            n = os.write(fd, report)
            if n != len(report):
                print(f"short write: {n}/{len(report)}", file=sys.stderr)
            sent += 1

            if args.verbose:
                print(f"#{sent} {report.hex()}")

            next_t += interval
            sleep_for = next_t - time.monotonic()
            if sleep_for > 0:
                time.sleep(sleep_for)
            else:
                # Falling behind — reset baseline so we don't spiral.
                next_t = time.monotonic()
    except KeyboardInterrupt:
        pass
    finally:
        os.close(fd)
        print(f"done. wrote {sent} reports.")


if __name__ == "__main__":
    main()
