#!/usr/bin/env python3
"""
ds-haptic-probe-win.py — Windows protocol-oracle probe for haptic-audio support.

Sibling of ds-haptic-probe.py. Same byte layout, same CLI, same patterns —
but uses HIDAPI userspace HID writes instead of /dev/hidraw, so it runs on
Windows (and on Linux too, against the same ESP32, for cross-validation).

What this answers
-----------------
The Linux probe proved the BLE 0x31 pipe carries arbitrary haptic bytes when
the host is /dev/hidraw on BlueZ. This script asks the same question of
Windows: does the Windows HID stack pass 0x31 output reports through to a
BLE HoGP peripheral and into the firmware's 24-byte haptic window?

DSX cannot answer this — DSX delivers haptics via USB Audio Class on Windows,
not HID. Most native-DualSense games and Steam Input *do* write 0x31 raw, so
proving the transport here is a foundation for everything that follows.

Usage
-----
    pip install hidapi          # (or: pip install hid)
    python ds-haptic-probe-win.py [--device <hidapi-path>]
                                  [--rate-hz 50] [--duration 5]
                                  [--pattern counter|sine|constant]
                                  [--offset N] [--length L] [--value V]
                                  [--no-crc]

Pairing prereq: ESP32 flashed with the haptic sniffer sketch and paired via
Settings -> Bluetooth -> "Wireless Controller". Watch the ESP32 serial
monitor (115200 baud) while running.

Notes
-----
Byte layout, CRC seed, and the 24-byte haptic window mirror the Linux probe.
See ds-haptic-probe.py for the per-byte breakdown of the 78-byte 0x31 BT
output report. The first byte of the buffer we hand to hidapi is the report
ID (0x31), which is what hidapi's `device.write()` expects on Windows.
"""

import argparse
import math
import struct
import sys
import time
import zlib
from typing import Optional

try:
    import hid
except ImportError:
    sys.exit("missing 'hid' module. install with:  pip install hidapi")

DS_REPORT_ID_BT  = 0x31
DS_OUTPUT_TAG    = 0x10
DS_REPORT_LEN_BT = 78
DS_CRC_SEED      = 0xA2  # BT output-report CRC seed prepended to the buffer

# Report 0x32 (haptic-audio sub-protocol) — 142 bytes total. Layout matches
# kijimad/soundsense and SAxense:
#   [0]      report_id 0x32
#   [1]      seq_tag (upper 4 bits seq, lower 4 tag)
#   [2-10]   sub-packet 0x11 (header 0x91, length 0x07, 7 control bytes incl. counter at byte 6)
#   [11-76]  sub-packet 0x12 (header 0x92, length 0x40, 64 audio bytes)
#   [77-137] 61 padding bytes (zero — slot for additional sub-packets)
#   [138-141] CRC32-LE over bytes 0..137 with seed 0xA2
DS_REPORT_ID_32     = 0x32
DS_REPORT_LEN_32    = 142
DS_PKT_AUDIO_OFFSET = 13   # default offset of 0x12 audio data within the 142-byte buffer
DS_PKT_AUDIO_LENGTH = 64   # canonical 0x12 sub-packet length (32 stereo frames)

DS_VID = 0x054C
DS_PID = 0x0CE6

GAMEPAD_USAGE_PAGE = 0x0001
GAMEPAD_USAGE      = 0x0005

# valid_flag0 bits (byte 3 of the 78-byte report / common_offset+0 in firmware).
# Mirrors DS_OUT_FLAG0_* in DualsenseGamepadDevice.h.
DS_VF0_HAPTICS_SELECT    = 0x02   # bit 1 — enable haptic motor select
DS_VF0_AUDIO_CONTROL     = 0x80   # bit 7 — marks audio_control byte as valid

# Sensible default for haptic-audio probing: tell the controller the
# audio_control field is valid and that haptic selection is active.
DS_VF0_HAPTIC_AUDIO_DEFAULT = DS_VF0_AUDIO_CONTROL | DS_VF0_HAPTICS_SELECT  # 0x82

# audio_control byte (buf[10], common_offset+7).
# Bit 4 routes audio to the controller's internal speaker circuit (LRA actuators).
DS_AUDIO_CTRL_INTERNAL_SPEAKER = 0x10


# ---------------------------------------------------------------------------
# Device discovery
# ---------------------------------------------------------------------------

def list_hid_candidates() -> list:
    """Return every HID interface hidapi can see, with the bits we care about
    for diagnostic output. Includes all VID/PID pairs, not just Sony, so the
    user can see what Windows actually exposed if our match fails."""
    out = []
    for entry in hid.enumerate():
        out.append({
            "path":         entry.get("path", b""),
            "vendor_id":    entry.get("vendor_id", 0),
            "product_id":   entry.get("product_id", 0),
            "usage_page":   entry.get("usage_page", 0),
            "usage":        entry.get("usage", 0),
            "product":      entry.get("product_string", "") or "",
            "manufacturer": entry.get("manufacturer_string", "") or "",
        })
    return out


def find_hid_path_for_dualsense() -> Optional[bytes]:
    """Pick the HID interface with VID/PID matching Sony DualSense. On BLE
    HoGP, Windows typically exposes one HID interface; prefer the Game Pad
    usage if multiple exist (e.g. composite descriptors)."""
    matches = [c for c in list_hid_candidates()
               if c["vendor_id"] == DS_VID and c["product_id"] == DS_PID]
    if not matches:
        return None
    for c in matches:
        if c["usage_page"] == GAMEPAD_USAGE_PAGE and c["usage"] == GAMEPAD_USAGE:
            return c["path"]
    return matches[0]["path"]


def _path_str(p) -> str:
    """hidapi paths are bytes on Linux, str on Windows depending on backend.
    Render uniformly for prints."""
    if isinstance(p, bytes):
        try:
            return p.decode("utf-8", errors="replace")
        except Exception:
            return repr(p)
    return str(p)


# ---------------------------------------------------------------------------
# Report construction (lifted verbatim from ds-haptic-probe.py)
# ---------------------------------------------------------------------------

def crc32_with_seed(buf: bytes, seed_byte: int) -> int:
    """DualSense BT CRC: zlib.crc32(seed || buf), little-endian."""
    return zlib.crc32(bytes([seed_byte]) + buf) & 0xFFFFFFFF


def build_report_0x32(payload_bytes: bytes,
                      offset: int,
                      seq: int,
                      counter: int,
                      with_crc: bool = True) -> bytes:
    """Construct a 142-byte 0x32 haptic-audio report. By default (offset=13,
    length=64) `payload_bytes` lands inside the 0x12 sub-packet's audio data
    region. Sub-packet headers and the 0x11 control bytes (incl. the frame
    counter at buf[10]) are always set so the firmware sub-packet parser
    finds them; the user payload overlays whatever range they specify, which
    is useful for pinpoint diagnostics (e.g. probing the padding region).
    """
    if offset < 2:
        raise ValueError("offset must be >= 2 (after report_id/seq_tag)")
    if offset + len(payload_bytes) > DS_REPORT_LEN_32 - (4 if with_crc else 0):
        raise ValueError("payload extends into CRC region")

    buf = bytearray(DS_REPORT_LEN_32)
    buf[0]  = DS_REPORT_ID_32
    buf[1]  = ((seq & 0x0F) << 4) | 0x00      # seq_tag

    # Sub-packet 0x11: control / engine state, 7 data bytes.
    buf[2]  = 0x91          # pid=0x11 | sized=0x80
    buf[3]  = 0x07          # length = 7
    buf[4]  = 0xFE          # enable flags (bits 1-7 set)
    # buf[5..8] = 0x00      # buffer state (zero — soundsense/SAxense default)
    buf[9]  = 0xFF          # end-of-buffer marker
    buf[10] = counter & 0xFF

    # Sub-packet 0x12: audio data, 64 bytes.
    buf[11] = 0x92          # pid=0x12 | sized=0x80
    buf[12] = 0x40          # length = 64

    # User payload overlay (defaults to bytes 13..76 = the 64-byte audio region).
    buf[offset:offset + len(payload_bytes)] = payload_bytes

    if with_crc:
        crc = crc32_with_seed(bytes(buf[:DS_REPORT_LEN_32 - 4]), DS_CRC_SEED)
        struct.pack_into("<I", buf, DS_REPORT_LEN_32 - 4, crc)
    return bytes(buf)


def build_report(payload_bytes: bytes,
                 offset: int,
                 seq: int,
                 with_crc: bool = True,
                 valid_flag0: int = DS_VF0_HAPTIC_AUDIO_DEFAULT,
                 audio_control: int = DS_AUDIO_CTRL_INTERNAL_SPEAKER,
                 audio_control2: int = 0x00) -> bytes:
    """Construct a 78-byte 0x31 BT output report with `payload_bytes` written
    starting at `offset`. The byte at index 0 is the report ID, so `offset`
    is relative to the start of the report including the report-id byte.

    Common-section flag bytes (set before CRC so they're covered by it):
      buf[3]  = valid_flag0   — which fields the controller should apply
      buf[10] = audio_control — speaker/LRA routing (bit 4 = internal speaker)
      buf[40] = audio_control2
    """
    if offset < 3:
        raise ValueError("offset must be >= 3 (after report_id/seq_tag/tag)")
    if offset + len(payload_bytes) > DS_REPORT_LEN_BT - (4 if with_crc else 0):
        raise ValueError("payload extends into CRC region")

    buf = bytearray(DS_REPORT_LEN_BT)
    buf[0]  = DS_REPORT_ID_BT
    buf[1]  = ((seq & 0x0F) << 4) | 0x00
    buf[2]  = DS_OUTPUT_TAG
    buf[3]  = valid_flag0 & 0xFF
    buf[10] = audio_control & 0xFF
    buf[40] = audio_control2 & 0xFF
    buf[offset:offset + len(payload_bytes)] = payload_bytes
    if with_crc:
        crc = crc32_with_seed(bytes(buf[:DS_REPORT_LEN_BT - 4]), DS_CRC_SEED)
        struct.pack_into("<I", buf, DS_REPORT_LEN_BT - 4, crc)
    return bytes(buf)


# ---------------------------------------------------------------------------
# Pattern generators (lifted verbatim from ds-haptic-probe.py)
# ---------------------------------------------------------------------------

def gen_counter(length: int, step: int) -> bytes:
    return bytes([(step & 0xFF)] * length)


def gen_sine(length: int, step: int, sample_rate_hz: float, freq_hz: float) -> bytes:
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
    ap.add_argument("--device",
                    help="hidapi device path (default: auto-detect Sony DS by VID/PID)")
    ap.add_argument("--report", type=lambda s: int(s, 0), default=DS_REPORT_ID_BT,
                    help="output report ID to write: 0x31 (default; 78-byte BT report "
                         "with 24-byte reserved-region haptic window) or 0x32 "
                         "(142-byte haptic-audio sub-protocol with 0x11 control + "
                         "0x12 audio sub-packets, matches SAxense / soundsense)")
    ap.add_argument("--rate-hz", type=float, default=50.0,
                    help="output reports per second (default: 50)")
    ap.add_argument("--duration", type=float, default=5.0,
                    help="seconds to run; 0 = run until Ctrl-C (default: 5)")
    ap.add_argument("--pattern", choices=("counter", "sine", "constant"),
                    default="counter",
                    help="payload pattern (default: counter -- most diagnosable)")
    ap.add_argument("--offset", type=int, default=None,
                    help="byte offset within the report (default: 50 for 0x31, "
                         "13 for 0x32 -- the start of each report's haptic payload)")
    ap.add_argument("--length", type=int, default=None,
                    help="payload length in bytes (default: 24 for 0x31, 64 for 0x32)")
    ap.add_argument("--value", type=lambda s: int(s, 0), default=0xAA,
                    help="constant byte value (default: 0xAA)")
    ap.add_argument("--sine-freq", type=float, default=200.0,
                    help="sine pattern frequency in Hz (default: 200)")
    ap.add_argument("--sine-srate", type=float, default=3000.0,
                    help="sine sample rate in Hz (default: 3000, matches "
                         "the rate documented for DS haptic audio)")
    ap.add_argument("--no-crc", action="store_true",
                    help="omit CRC32 trailer (firmware doesn't validate output CRC)")
    ap.add_argument("--vf0", type=lambda s: int(s, 0),
                    default=DS_VF0_HAPTIC_AUDIO_DEFAULT,
                    help="valid_flag0 byte (default: 0x82 = AUDIO_CONTROL|HAPTICS_SELECT)")
    ap.add_argument("--audio-ctrl", type=lambda s: int(s, 0),
                    default=DS_AUDIO_CTRL_INTERNAL_SPEAKER,
                    help="audio_control byte (default: 0x10 = internal speaker/LRA routing)")
    ap.add_argument("--audio-ctrl2", type=lambda s: int(s, 0), default=0x00,
                    help="audio_control2 byte (default: 0x00)")
    ap.add_argument("--verbose", action="store_true",
                    help="print every report we write (slow, debugging only)")
    args = ap.parse_args()

    # Per-report defaults for offset/length when the user didn't override them.
    if args.report == DS_REPORT_ID_32:
        if args.offset is None: args.offset = DS_PKT_AUDIO_OFFSET   # 13
        if args.length is None: args.length = DS_PKT_AUDIO_LENGTH   # 64
    else:
        if args.offset is None: args.offset = 50
        if args.length is None: args.length = 24

    if args.device is not None:
        path_arg = args.device
        device_path = path_arg.encode("utf-8") if isinstance(path_arg, str) else path_arg
    else:
        device_path = find_hid_path_for_dualsense()

    if not device_path:
        candidates = list_hid_candidates()
        if not candidates:
            sys.exit("hidapi reports zero HID devices. Is the ESP32 paired and "
                     "connected? Settings -> Bluetooth -> 'Wireless Controller' "
                     "should show as Connected. Try unpair/repair if it's not.")
        print("no HID interface matched Sony DualSense (054C:0CE6).", file=sys.stderr)
        print("HID interfaces hidapi currently sees:", file=sys.stderr)
        for c in candidates:
            print(f"  VID:{c['vendor_id']:04X} PID:{c['product_id']:04X} "
                  f"UP:{c['usage_page']:04X} U:{c['usage']:04X}  "
                  f"{c['manufacturer']!r} / {c['product']!r}",
                  file=sys.stderr)
            print(f"    path: {_path_str(c['path'])}", file=sys.stderr)
        sys.exit("if one of these is your ESP32, re-run with --device <path>.")

    print(f"using {_path_str(device_path)}")

    try:
        dev = hid.device()
        dev.open_path(device_path)
    except (OSError, IOError) as e:
        sys.exit(f"open_path failed: {e}")

    print(f"writing report 0x{args.report:02X}, {args.pattern} pattern at "
          f"offset={args.offset} length={args.length} rate={args.rate_hz} Hz "
          f"crc={'no' if args.no_crc else 'yes'}")
    print("watch the ESP32 serial monitor; sniffer's [stats] line should "
          "report diff>0 for these writes")

    interval = 1.0 / args.rate_hz
    end_at = (time.monotonic() + args.duration) if args.duration > 0 else None
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
            else:
                payload = gen_constant(args.length, args.value)

            if args.report == DS_REPORT_ID_32:
                report = build_report_0x32(payload, args.offset, sent,
                                           counter=sent,
                                           with_crc=not args.no_crc)
            else:
                report = build_report(payload, args.offset, sent,
                                      with_crc=not args.no_crc,
                                      valid_flag0=args.vf0,
                                      audio_control=args.audio_ctrl,
                                      audio_control2=args.audio_ctrl2)
            try:
                n = dev.write(report)
            except (OSError, IOError) as e:
                print(f"write failed after {sent} reports: {e}", file=sys.stderr)
                break
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
                next_t = time.monotonic()
    except KeyboardInterrupt:
        pass
    finally:
        try:
            dev.close()
        except Exception:
            pass
        print(f"done. wrote {sent} reports.")


if __name__ == "__main__":
    main()
