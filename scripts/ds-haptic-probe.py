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
import subprocess
import sys
import time
import zlib
from typing import Optional

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

# Report 0x36 (larger haptic-audio variant from recent Unreal-Dualsense builds)
# — 398 bytes total, same 0x11 + 0x12 sub-packet protocol as 0x32 but in a
# wider buffer so you can fit a much larger 0x12 audio payload per packet.
# That cuts the per-second report rate (e.g. 25 Hz at 240-byte audio vs ~94 Hz
# at 64-byte audio), reducing BLE / NimBLE per-packet overhead on the ESP32.
DS_REPORT_ID_36         = 0x36
DS_REPORT_LEN_36        = 398
DS_PKT_AUDIO_OFFSET_36  = 13   # same offset; only the trailing region grows
DS_PKT_AUDIO_LENGTH_36  = 240  # default 0x12 length for 0x36 — 120 stereo
                               # frames @ 3 kHz, well under the 255-byte
                               # single-sub-packet length limit (uint8 length)

DS_VID = 0x054C
DS_PID = 0x0CE6

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

# Defaults for audio-file piping mode (matches SAxense / soundsense and the
# firmware's int8 stereo expectation; see DualsenseGamepadDevice.h:160-192).
DS_HAPTIC_AUDIO_DEFAULT_RATE_HZ  = 3000   # SAxense / soundsense canonical rate
DS_HAPTIC_AUDIO_DEFAULT_CHANNELS = 2      # stereo interleaved (L8 R8 L8 R8 ...)
DS_HAPTIC_AUDIO_DEFAULT_FORMAT   = "s8"   # firmware reads int8_t


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


def _build_haptic_subpacket_report(payload_bytes: bytes,
                                   offset: int,
                                   seq: int,
                                   counter: int,
                                   report_id: int,
                                   total_size: int,
                                   with_crc: bool = True) -> bytes:
    """Shared builder for 0x32 (142-byte) and 0x36 (398-byte) haptic-audio
    reports. Both use the same 0x11 + 0x12 sub-packet protocol; only the
    overall buffer size differs (0x36 has room for a larger 0x12 audio
    sub-packet or additional sub-packets like 0x15 headset-audio).

    By default (offset=13) `payload_bytes` lands inside the 0x12 sub-packet's
    audio region. The 0x12 length byte is set to len(payload_bytes), so the
    firmware parser pulls exactly the bytes we wrote — no zero padding read
    as fake audio. Max single 0x12 length is 255 (uint8 length field).
    """
    if offset < 2:
        raise ValueError("offset must be >= 2 (after report_id/seq_tag)")
    if offset + len(payload_bytes) > total_size - (4 if with_crc else 0):
        raise ValueError("payload extends into CRC region")
    if len(payload_bytes) > 255:
        raise ValueError("0x12 sub-packet length field is uint8 (max 255 bytes)")

    buf = bytearray(total_size)
    buf[0]  = report_id
    buf[1]  = ((seq & 0x0F) << 4) | 0x00      # seq_tag

    # Sub-packet 0x11: control / engine state, 7 data bytes.
    buf[2]  = 0x91          # pid=0x11 | sized=0x80
    buf[3]  = 0x07          # length = 7
    buf[4]  = 0xFE          # enable flags (bits 1-7 set)
    # buf[5..8] = 0x00      # buffer state (zero — soundsense/SAxense default)
    buf[9]  = 0xFF          # end-of-buffer marker
    buf[10] = counter & 0xFF

    # Sub-packet 0x12: audio data, length matches the user payload.
    buf[11] = 0x92                       # pid=0x12 | sized=0x80
    buf[12] = len(payload_bytes) & 0xFF  # length

    buf[offset:offset + len(payload_bytes)] = payload_bytes

    if with_crc:
        crc = crc32_with_seed(bytes(buf[:total_size - 4]), DS_CRC_SEED)
        struct.pack_into("<I", buf, total_size - 4, crc)
    return bytes(buf)


def build_report_0x32(payload_bytes: bytes,
                      offset: int,
                      seq: int,
                      counter: int,
                      with_crc: bool = True) -> bytes:
    """142-byte 0x32 haptic-audio report (SAxense / soundsense / older
    Unreal-Dualsense)."""
    return _build_haptic_subpacket_report(payload_bytes, offset, seq, counter,
                                          report_id=DS_REPORT_ID_32,
                                          total_size=DS_REPORT_LEN_32,
                                          with_crc=with_crc)


def build_report_0x36(payload_bytes: bytes,
                      offset: int,
                      seq: int,
                      counter: int,
                      with_crc: bool = True) -> bytes:
    """398-byte 0x36 haptic-audio report (recent Unreal-Dualsense). Same
    sub-packet protocol as 0x32, larger buffer — fits much bigger 0x12
    audio sub-packets, which lowers the per-second report rate and reduces
    BLE / NimBLE per-packet overhead on the ESP32."""
    return _build_haptic_subpacket_report(payload_bytes, offset, seq, counter,
                                          report_id=DS_REPORT_ID_36,
                                          total_size=DS_REPORT_LEN_36,
                                          with_crc=with_crc)


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
    buf[1]  = ((seq & 0x0F) << 4) | 0x00      # upper nibble seq, lower tag
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
# Audio-file source (ffmpeg pipe)
# ---------------------------------------------------------------------------

def open_audio_stream(path: str,
                      sample_rate: int,
                      channels: int,
                      audio_format: str,
                      loop: bool,
                      volume_db: float,
                      ffmpeg_bin: str,
                      monitor_driver: Optional[str] = None,
                      monitor_device: str = "default",
                      monitor_rate: int = 44100,
                      monitor_channels: int = 2) -> subprocess.Popen:
    """Spawn ffmpeg decoding `path` to raw PCM on stdout. We deliberately do
    NOT pass `-re`: the main loop already paces sends via time.sleep, and
    ffmpeg will block on pipe backpressure naturally — adding `-re` here
    would double-pace and drift against our schedule.

    When `monitor_driver` is set (e.g. "pulse"), ffmpeg is given a second
    output that plays locally at full quality via that driver. The haptic
    pipe remains on stdout (pipe:1). ffmpeg's pulse muxer thread runs
    independently, so local audio stays smooth regardless of Python's pacing."""
    cmd = [ffmpeg_bin, "-hide_banner", "-loglevel", "error"]
    if loop:
        cmd += ["-stream_loop", "-1"]
    cmd += ["-i", path]

    if monitor_driver:
        # Two outputs require explicit -map per output. Apply volume (if any)
        # via asplit so both branches share the same filter graph.
        if volume_db != 0.0:
            cmd += ["-filter_complex",
                    f"[0:a]volume={volume_db}dB,asplit=2[h][m]"]
            cmd += ["-map", "[h]"]   # haptic pipe
        else:
            cmd += ["-map", "0:a"]   # haptic pipe
        cmd += ["-ac", str(channels), "-ar", str(sample_rate),
                "-f", audio_format, "pipe:1"]

        if volume_db != 0.0:
            cmd += ["-map", "[m]"]   # monitor
        else:
            cmd += ["-map", "0:a"]   # monitor
        cmd += ["-ac", str(monitor_channels), "-ar", str(monitor_rate),
                "-f", monitor_driver, monitor_device]
    else:
        if volume_db != 0.0:
            cmd += ["-af", f"volume={volume_db}dB"]
        cmd += ["-ac", str(channels), "-ar", str(sample_rate),
                "-f", audio_format, "-"]

    try:
        proc = subprocess.Popen(cmd,
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                bufsize=0)
    except FileNotFoundError:
        sys.exit(f"ffmpeg not found at {ffmpeg_bin!r}. Install ffmpeg or "
                 "pass --ffmpeg /path/to/ffmpeg.")
    return proc


def read_audio_chunk(proc: subprocess.Popen,
                     length: int,
                     silence_byte: int) -> tuple:
    """Read exactly `length` bytes from the ffmpeg pipe. On EOF, pad the
    remainder with silence and return (chunk, eof=True)."""
    buf = bytearray()
    while len(buf) < length:
        part = proc.stdout.read(length - len(buf))
        if not part:
            buf.extend(bytes([silence_byte]) * (length - len(buf)))
            return bytes(buf), True
        buf.extend(part)
    return bytes(buf), False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--device", help="hidraw node (default: auto-detect Sony DS)")
    ap.add_argument("--report", type=lambda s: int(s, 0), default=DS_REPORT_ID_BT,
                    help="output report ID to write: 0x31 (default; 78-byte BT report "
                         "with 24-byte reserved-region haptic window), 0x32 "
                         "(142-byte haptic-audio sub-protocol with 0x11 control + "
                         "0x12 audio sub-packets, matches SAxense / soundsense), or "
                         "0x36 (398-byte larger haptic-audio variant from recent "
                         "Unreal-Dualsense — same sub-packet protocol but accepts "
                         "much bigger 0x12 audio payloads, lowering report rate "
                         "and BLE/NimBLE per-packet overhead on the ESP32)")
    ap.add_argument("--rate-hz", type=float, default=None,
                    help="output reports per second (default: 50 for synthetic "
                         "patterns; auto-derived from --audio-rate / "
                         "frames-per-packet for audio mode)")
    ap.add_argument("--duration", type=float, default=5.0,
                    help="seconds to run; 0 = run until Ctrl-C (default: 5)")
    ap.add_argument("--pattern", choices=("counter", "sine", "constant", "audio"),
                    default="counter",
                    help="payload pattern (default: counter — most diagnosable; "
                         "use 'audio' or pass --audio-file to stream a decoded "
                         "audio file via ffmpeg)")
    ap.add_argument("--offset", type=int, default=None,
                    help="byte offset within the report (default: 50 for 0x31, "
                         "13 for 0x32 / 0x36 — the start of each report's "
                         "haptic payload)")
    ap.add_argument("--length", type=int, default=None,
                    help="payload length in bytes (default: 24 for 0x31, 64 for "
                         "0x32, 240 for 0x36 — 0x36's larger buffer fits a much "
                         "bigger 0x12 sub-packet up to the uint8 length max of 255)")
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
    ap.add_argument("--audio-file",
                    help="path to an audio file to decode via ffmpeg and stream "
                         "as the payload (any format ffmpeg can read). Implies "
                         "--pattern audio; loops by default. See SAxense / "
                         "soundsense for the reference flow.")
    ap.add_argument("--audio-rate", type=int,
                    default=DS_HAPTIC_AUDIO_DEFAULT_RATE_HZ,
                    help=f"audio sample rate fed to ffmpeg "
                         f"(default: {DS_HAPTIC_AUDIO_DEFAULT_RATE_HZ} Hz, the "
                         f"SAxense / soundsense canonical haptic-audio rate)")
    ap.add_argument("--audio-channels", type=int, choices=(1, 2),
                    default=DS_HAPTIC_AUDIO_DEFAULT_CHANNELS,
                    help=f"audio channel count "
                         f"(default: {DS_HAPTIC_AUDIO_DEFAULT_CHANNELS}; firmware "
                         f"expects stereo interleaved)")
    ap.add_argument("--audio-format", choices=("s8", "u8"),
                    default=DS_HAPTIC_AUDIO_DEFAULT_FORMAT,
                    help=f"raw PCM sample format requested from ffmpeg "
                         f"(default: {DS_HAPTIC_AUDIO_DEFAULT_FORMAT}; firmware "
                         f"reads int8_t so s8 is correct)")
    ap.add_argument("--audio-no-loop", action="store_true",
                    help="play the audio file once and stop (default: loop forever)")
    ap.add_argument("--audio-volume-db", type=float, default=0.0,
                    help="apply an ffmpeg volume filter in dB (default: 0.0 = unity)")
    ap.add_argument("--ffmpeg", default="ffmpeg",
                    help="ffmpeg binary path (default: ffmpeg from PATH)")
    ap.add_argument("--monitor", action="store_true",
                    help="play audio locally while sending haptic data. Adds a "
                         "second ffmpeg output at full quality (44100 Hz) routed "
                         "via --monitor-driver. Only valid with --audio-file.")
    ap.add_argument("--monitor-driver",
                    choices=("pulse", "alsa", "pipewire"),
                    default="pulse",
                    help="ffmpeg audio driver for local monitoring "
                         "(default: pulse; PipeWire intercepts this transparently)")
    ap.add_argument("--monitor-device", default="default",
                    help="output device passed to the monitor driver "
                         "(default: default)")
    ap.add_argument("--verbose", action="store_true",
                    help="print every report we write (slow, debugging only)")
    args = ap.parse_args()

    # Per-report defaults for offset/length when the user didn't override them.
    if args.report == DS_REPORT_ID_32:
        if args.offset is None: args.offset = DS_PKT_AUDIO_OFFSET      # 13
        if args.length is None: args.length = DS_PKT_AUDIO_LENGTH      # 64
    elif args.report == DS_REPORT_ID_36:
        if args.offset is None: args.offset = DS_PKT_AUDIO_OFFSET_36   # 13
        if args.length is None: args.length = DS_PKT_AUDIO_LENGTH_36   # 240
    else:
        if args.offset is None: args.offset = 50
        if args.length is None: args.length = 24

    # --audio-file implies the audio pattern; the inverse must also be valid.
    if args.audio_file:
        args.pattern = "audio"
    if args.pattern == "audio" and not args.audio_file:
        sys.exit("--pattern audio requires --audio-file PATH")

    # Auto-derive report rate from the audio config when not set explicitly,
    # otherwise audio plays at the wrong speed (loop pacing != sample rate).
    if args.rate_hz is None:
        if args.pattern == "audio":
            bytes_per_frame = args.audio_channels   # s8/u8 = 1 byte/sample
            frames_per_packet = args.length // bytes_per_frame
            if frames_per_packet < 1:
                sys.exit(f"--length {args.length} too small for "
                         f"{args.audio_channels}-channel audio")
            args.rate_hz = args.audio_rate / frames_per_packet
            print(f"audio mode: auto-derived --rate-hz = {args.rate_hz:.3f} "
                  f"({args.audio_rate} Hz / {frames_per_packet} frames per packet)")
        else:
            args.rate_hz = 50.0

    audio_proc = None
    silence_byte = 0x00 if args.audio_format == "s8" else 0x80
    if args.pattern == "audio":
        monitor_driver = args.monitor_driver if args.monitor else None
        if monitor_driver:
            print(f"monitoring locally via ffmpeg -{monitor_driver} "
                  f"({args.monitor_device}, 44100 Hz stereo)")
        audio_proc = open_audio_stream(args.audio_file,
                                       args.audio_rate,
                                       args.audio_channels,
                                       args.audio_format,
                                       loop=not args.audio_no_loop,
                                       volume_db=args.audio_volume_db,
                                       ffmpeg_bin=args.ffmpeg,
                                       monitor_driver=monitor_driver,
                                       monitor_device=args.monitor_device)

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

    print(f"writing report 0x{args.report:02X}, {args.pattern} pattern at "
          f"offset={args.offset} length={args.length} rate={args.rate_hz} Hz "
          f"crc={'no' if args.no_crc else 'yes'}")
    print("watch the ESP32 serial monitor; sniffer's [stats] line should "
          "report diff>0 for these writes")

    sent = 0
    next_t = time.monotonic()
    audio_eof = False
    try:
        while True:
            if end_at and time.monotonic() >= end_at:
                break

            if args.pattern == "counter":
                payload = gen_counter(args.length, sent)
            elif args.pattern == "sine":
                payload = gen_sine(args.length, sent,
                                   args.sine_srate, args.sine_freq)
            elif args.pattern == "audio":
                payload, audio_eof = read_audio_chunk(audio_proc, args.length,
                                                      silence_byte)
            else:  # constant
                payload = gen_constant(args.length, args.value)

            if args.report == DS_REPORT_ID_32:
                report = build_report_0x32(payload, args.offset, sent,
                                           counter=sent,
                                           with_crc=not args.no_crc)
            elif args.report == DS_REPORT_ID_36:
                report = build_report_0x36(payload, args.offset, sent,
                                           counter=sent,
                                           with_crc=not args.no_crc)
            else:
                report = build_report(payload, args.offset, sent,
                                      with_crc=not args.no_crc,
                                      valid_flag0=args.vf0,
                                      audio_control=args.audio_ctrl,
                                      audio_control2=args.audio_ctrl2)
            n = os.write(fd, report)
            if n != len(report):
                print(f"short write: {n}/{len(report)}", file=sys.stderr)
            sent += 1

            if args.verbose:
                print(f"#{sent} {report.hex()}")

            # In non-looping audio mode, send the final padded chunk then exit.
            if audio_eof:
                break

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
        if audio_proc is not None:
            try:
                audio_proc.stdout.close()
            except Exception:
                pass
            audio_proc.terminate()
            try:
                audio_proc.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                audio_proc.kill()
            err = (audio_proc.stderr.read().decode("utf-8", "replace").strip()
                   if audio_proc.stderr else "")
            # rc < 0 means signal-killed (e.g. SIGTERM from us) — that's normal.
            if err and (audio_proc.returncode or 0) > 0:
                print(f"ffmpeg: {err}", file=sys.stderr)
        print(f"done. wrote {sent} reports.")


if __name__ == "__main__":
    main()
