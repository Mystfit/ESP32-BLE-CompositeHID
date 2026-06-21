#ifndef DUALSENSE_GAMEPAD_DEVICE_H
#define DUALSENSE_GAMEPAD_DEVICE_H

#include <cstddef>
#include <Callback.h>
#include <NimBLECharacteristic.h>
#include <cstdint>
#include <mutex>

#include "BLEHostConfiguration.h"
#include "BaseCompositeDevice.h"
#include "DualsenseDescriptors.h"
#include "DualsenseGamepadConfiguration.h"
#include "GamepadDevice.h"
// Button bitmasks
#define DUALSENSE_BUTTON_Y 0x08
#define DUALSENSE_BUTTON_B 0x04
#define DUALSENSE_BUTTON_A 0x02
#define DUALSENSE_BUTTON_X 0x01

#define DUALSENSE_BUTTON_DPAD_NONE 0x08
#define DUALSENSE_BUTTON_DPAD_NORTH 0x00
#define DUALSENSE_BUTTON_DPAD_NORTHEAST 0x01
#define DUALSENSE_BUTTON_DPAD_EAST 0x02
#define DUALSENSE_BUTTON_DPAD_SOUTHEAST 0x03
#define DUALSENSE_BUTTON_DPAD_SOUTH 0x04
#define DUALSENSE_BUTTON_DPAD_SOUTHWEST 0x05
#define DUALSENSE_BUTTON_DPAD_WEST 0x06
#define DUALSENSE_BUTTON_DPAD_NORTHWEST 0x07
#define DUALSENSE_BUTTON_LB 0x10
#define DUALSENSE_BUTTON_RB 0x20
#define DUALSENSE_BUTTON_LT 0x40
#define DUALSENSE_BUTTON_RT 0x80
#define DUALSENSE_BUTTON_SELECT 0x100
#define DUALSENSE_BUTTON_START 0x200
#define DUALSENSE_BUTTON_LS 0x400
#define DUALSENSE_BUTTON_RS 0x800

#define DUALSENSE_BUTTON_MODE 0x1000
#define DUALSENSE_BUTTON_TOUCHPAD 0x2000
#define DUALSENSE_BUTTON_SHARE 0x4000
#define DUALSENSE_BUTTON_MUTE 0x8000
#define DUALSENSE_BUTTON_L4 0x10000
#define DUALSENSE_BUTTON_R4 0x20000
#define DUALSENSE_BUTTON_L5 0x40000
#define DUALSENSE_BUTTON_R5 0x80000

// Dpad values

// Dpad bitflags
enum DualsenseDpadFlags : uint8_t {
    NONE = 0x08,
    NORTH = 0x00,
    EAST = 0x02,
    SOUTH = 0x04,
    WEST = 0x08
};

// Trigger range
#define DUALSENSE_TRIGGER_MIN 0
#define DUALSENSE_TRIGGER_MAX 255

// Battery level range (0-100, representing 0%-100%)
#define DUALSENSE_BATTERY_MIN 0
#define DUALSENSE_BATTERY_MAX 100

// Charging status range (high nibble of status byte)
#define DUALSENSE_CHARGING_MIN 0
#define DUALSENSE_CHARGING_MAX 15

// Thumbstick range
#define DUALSENSE_STICK_MIN -127
#define DUALSENSE_STICK_MAX 127
#define DUALSENSE_ACC_RES_PER_G 8192
#define DUALSENSE_ACC_RANGE (4 * 8192)
#define DUALSENSE_GYRO_RES_PER_DEG_S 1024
#define DUALSENSE_GYRO_RANGE (2048 * 1024)
#define DUALSENSE_AXIS_CENTER_OFFSET 0x80

// player leds masks
#define DUALSENSE_PLAYERLED_ON 0x20
#define DUALSENSE_PLAYERLED_1 0x01
#define DUALSENSE_PLAYERLED_2 0x02
#define DUALSENSE_PLAYERLED_3 0x04
#define DUALSENSE_PLAYERLED_4 0x08
#define DUALSENSE_PLAYERLED_5 0x10

// Output-report valid-flag bits (bit masks for valid_flag0 / valid_flag1 / valid_flag2).
// The host sets a bit to indicate that the corresponding field in the output report
// should be applied; unset bits mean "ignore this field". Mirrors Linux hid-playstation.c.
#define DS_OUT_FLAG0_COMPATIBLE_VIBRATION    (1 << 0)
#define DS_OUT_FLAG0_HAPTICS_SELECT          (1 << 1)
#define DS_OUT_FLAG0_RIGHT_TRIGGER_EFFECT    (1 << 2)
#define DS_OUT_FLAG0_LEFT_TRIGGER_EFFECT     (1 << 3)
#define DS_OUT_FLAG0_HEADPHONE_VOLUME        (1 << 4)
#define DS_OUT_FLAG0_SPEAKER_VOLUME          (1 << 5)
#define DS_OUT_FLAG0_MIC_VOLUME              (1 << 6)
#define DS_OUT_FLAG0_AUDIO_CONTROL           (1 << 7)

#define DS_OUT_FLAG1_MIC_MUTE_LED            (1 << 0)
#define DS_OUT_FLAG1_POWER_SAVE_CONTROL      (1 << 1)
#define DS_OUT_FLAG1_LIGHTBAR                (1 << 2)
#define DS_OUT_FLAG1_RELEASE_LEDS            (1 << 3)
#define DS_OUT_FLAG1_PLAYER_INDICATOR        (1 << 4)
#define DS_OUT_FLAG1_AUDIO_CONTROL2          (1 << 7)

#define DS_OUT_FLAG2_LIGHTBAR_SETUP          (1 << 1)
#define DS_OUT_FLAG2_COMPATIBLE_VIBRATION2   (1 << 2)

// Adaptive-trigger motor effect modes (value of right/left_trigger_motor_mode).
// Empirically confirmed from DSX captures. Vibration is the BLE-reachable
// haptic proxy: params carry frequency + amplitude. Feedback also encodes
// Slope and Multi-Position depending on the bitmask pattern in raw_params[0..1]:
//   Feedback / Slope  : contiguous bits from start_position to bit 9
//   Multiple Position : any arbitrary combination of bits
enum class DsTriggerMode : uint8_t {
    Off       = 0x05,
    Feedback  = 0x21,  // also encodes Slope and Multi-Position
    Weapon    = 0x25,
    Vibration = 0x26,
};

// Deprecated: prefer DsTriggerMode. Will be removed in a future release.
#define DS_TRIGGER_EFFECT_OFF       static_cast<uint8_t>(DsTriggerMode::Off)
#define DS_TRIGGER_EFFECT_FEEDBACK  static_cast<uint8_t>(DsTriggerMode::Feedback)
#define DS_TRIGGER_EFFECT_WEAPON    static_cast<uint8_t>(DsTriggerMode::Weapon)
#define DS_TRIGGER_EFFECT_VIBRATION static_cast<uint8_t>(DsTriggerMode::Vibration)

// Fully-classified sub-effect type returned by ParsedTriggerEffect::subtype().
// DSX multiplexes several logical effects onto a single wire mode byte; this
// enum exposes the resolved variant so callers can switch on one value without
// replicating bitmask-contiguity or params[9] detection logic themselves.
enum DsTriggerEffectSubtype : uint8_t {
    DS_TRIGGER_SUBTYPE_OFF,
    DS_TRIGGER_SUBTYPE_FEEDBACK,
    DS_TRIGGER_SUBTYPE_SLOPE_FEEDBACK,
    DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_FEEDBACK,
    DS_TRIGGER_SUBTYPE_WEAPON,
    DS_TRIGGER_SUBTYPE_VIBRATION,
    DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_VIBRATION,
    DS_TRIGGER_SUBTYPE_UNKNOWN,
};

// Forwards
class DualsenseGamepadDevice;

class DualsenseGamepadCallbacks : public NimBLECharacteristicCallbacks {
public:
    DualsenseGamepadCallbacks(DualsenseGamepadDevice* device);

    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;

private:
    DualsenseGamepadDevice* _device;
};

// Haptic-audio frame extracted from a DualSense BT 0x31 output report.
//
// The DualSense BT output report's 24-byte "reserved4" region (bytes 50-73 of
// the 78-byte wire form) is empirically forwarded byte-for-byte by Linux's
// BlueZ HID-over-GATT bridge to BLE peripherals — verified with
// scripts/ds-haptic-probe.py against this firmware. We treat that window as
// 8-bit signed PCM, stereo interleaved (L0 R0 L1 R1 ... L11 R11 = 12 stereo
// frames per packet). Sample rate is host-defined; at the firmware level we
// just expose what arrived. A typical writer at 100 Hz packet rate yields
// ~1.2 kHz stereo, sufficient for low-frequency VCA / LRA haptic actuators.
//
// Pointers are zero-copy slices into the live BLE buffer and are ONLY valid
// for the duration of the onHapticAudioReceived callback. Copy out (e.g. into
// a ring buffer) if you need to consume them asynchronously.
//
// This is a contract our emulator defines: any host that wants haptic audio
// to reach the controller writes 8-bit signed PCM into bytes 50-73 of the
// 0x31 output report. Real PS5-targeted hosts (DSX-on-Windows) currently do
// NOT use this path — see the project notes in
// examples/dualsenseExamples/Dualsense_Edge_Controller.
struct HapticAudioFrame {
    const int8_t*  samples           = nullptr;  // interleaved L R L R ...
    size_t         sampleCount       = 0;        // per-channel; total bytes = sampleCount * 2
    uint8_t        audioControlBits  = 0;        // mirrors output report audio_control (0x31-source only)
    uint8_t        audioControl2Bits = 0;        // mirrors output report audio_control2 (0x31-source only)
    uint8_t        seq               = 0;        // upper nibble of seq_tag

    // 0x32 sub-packet 0x11 fields (zero for 0x31-source frames). controlCounter
    // is the haptic engine's frame counter — increments per packet on the host
    // side; useful for drop / reorder detection.
    uint8_t        controlEnable     = 0;        // 0x11 sub-packet byte 0 (typically 0xFE)
    uint8_t        controlCounter    = 0;        // 0x11 sub-packet byte 6
};

// DualSense BT Output Report parsed view (wire format is 78 bytes).
// Based on Linux hid-playstation.c dualsense_output_report_bt struct.
// This is NOT a packed wire struct: load() decodes the raw bytes field by
// field, so the struct layout doesn't need to match the wire layout.
struct DualsenseGamepadOutputReportData {
    // BT Header (3 bytes)
    uint8_t report_id = 0;          // Byte 0: Should be 0x31
    uint8_t seq_tag = 0;            // Byte 1: Sequence tag (upper 4 bits = seq, lower 4 = tag)
    uint8_t tag = 0;                // Byte 2: Should be 0x10 (DS_OUTPUT_TAG)

    // Common section starts at byte 3 (47 bytes)
    uint8_t valid_flag0 = 0;        // Byte 3
    uint8_t valid_flag1 = 0;        // Byte 4
    uint8_t motor_right = 0;        // Byte 5: Strong motor
    uint8_t motor_left = 0;         // Byte 6: Weak motor

    uint8_t headphone_volume = 0;   // Byte 7
    uint8_t speaker_volume = 0;     // Byte 8
    uint8_t mic_volume = 0;         // Byte 9
    uint8_t audio_control = 0;      // Byte 10
    uint8_t mute_button_led = 0;    // Byte 11

    uint8_t power_save_control = 0; // Byte 12

    // Adaptive-trigger effect blocks (canonical layout from hid-playstation.c).
    // Each trigger has 1 mode byte + 10 parameter bytes. For DS_TRIGGER_EFFECT_VIBRATION
    // (0x26): params[0]=frequency (Hz), params[1]=amplitude (0..8), params[2]=start pos.
    uint8_t right_trigger_motor_mode = 0;         // Byte 13
    uint8_t right_trigger_param[10]  = { 0 };     // Bytes 14-23
    uint8_t left_trigger_motor_mode  = 0;         // Byte 24
    uint8_t left_trigger_param[10]   = { 0 };     // Bytes 25-34
    uint8_t reserved_host_timestamp[4] = { 0 };   // Bytes 35-38
    uint8_t reduce_motor_power       = 0;         // Byte 39

    uint8_t audio_control2 = 0;     // Byte 40

    uint8_t valid_flag2 = 0;        // Byte 41
    uint8_t reserved3[2] = { 0 };   // Bytes 42-43

    uint8_t lightbar_setup = 0;     // Byte 44
    uint8_t led_brightness = 0;     // Byte 45
    uint8_t player_leds = 0;        // Byte 46
    uint8_t lightbar_red = 0;       // Byte 47
    uint8_t lightbar_green = 0;     // Byte 48
    uint8_t lightbar_blue = 0;      // Byte 49

    uint8_t reserved4[24] = { 0 };  // Bytes 50-73
    uint32_t crc32 = 0;             // Bytes 74-77

    // Raw view of the BLE output-report buffer that produced this struct.
    // Populated by load(); used by the haptic-audio sniffer and by the
    // (future) hapticAudio() accessor to read regions outside the named
    // fields. Pointer is borrowed from the live BLE buffer and is ONLY
    // valid for the duration of the onReceivedOutputReport callback.
    // common_offset_used is the offset into raw_data where the 47-byte
    // common section begins (selected by load() based on wire format).
    // For report 0x32 (haptic-audio sub-protocol), there is no common
    // section: common_offset_used is set to -1 as a sentinel.
    const uint8_t* raw_data            = nullptr;
    size_t         raw_size            = 0;
    int            common_offset_used  = 0;

    // Sub-packet pointers populated by load() when parsing report 0x32.
    // Both nullptr for any other report ID. Pointers borrowed from raw_data
    // and only valid for the lifetime of the onReceivedOutputReport callback.
    const uint8_t* haptic_pkt12_data   = nullptr;  // 0x12 sub-packet payload (audio bytes, int8 stereo)
    size_t         haptic_pkt12_length = 0;        // bytes (typically 64 = 32 stereo frames)
    const uint8_t* haptic_pkt11_data   = nullptr;  // 0x11 sub-packet payload (control / engine state)
    size_t         haptic_pkt11_length = 0;        // bytes (typically 7)

    // Decoded view of a Feedback effect (DS_TRIGGER_EFFECT_FEEDBACK, 0x21).
    // Wire encoding:
    //   params[0..1] : 10-bit LE bitmask — bit N = position N active.
    //                  Positions >= start_position are set.
    //   params[2..5] : 30-bit packed field, 3 bits per position.
    //                  strength[pos] = (packed32 >> (pos*3)) & 7  (0-7 internal, 0 = min).
    //                  DSX "Resistance N" maps to internal value N-1.
    //   params[6..9] : unused / zero for this mode.
    struct FeedbackTriggerEffect {
        uint8_t  start_position;             // lowest active position in the bitmask (0-9)
        uint16_t position_mask;              // raw 10-bit field; bit N = position N is active
        uint8_t  per_position_strength[10];  // 0 = inactive; 1-8 = resistance at that position
        const uint8_t* raw_params;           // raw 10-byte param array for advanced use

        // Strength of the first active position (0 if none).
        // In the common single-strength case (DSX) this is the uniform resistance level.
        uint8_t strength() const {
            for (int i = 0; i < 10; i++)
                if (per_position_strength[i]) return per_position_strength[i];
            return 0;
        }
    };

    // Decoded view of a Weapon effect (DS_TRIGGER_EFFECT_WEAPON, 0x25).
    // Wire encoding:
    //   params[0..1] : 10-bit LE bitmask with exactly two bits set:
    //                  the start_position bit and the end_position bit.
    //                  start = lowest set bit, end = highest set bit.
    //   params[2]    : resistance strength, 0-7 internal (= DSX strength - 1).
    //   params[3..9] : unused / zero.
    // Note: DSX clamps start to (end-1) when start >= end, so identical bytes
    // may be seen for different UI start values when start >= end.
    struct WeaponTriggerEffect {
        uint8_t  start_position;  // 0-9: where the trigger goes stiff
        uint8_t  end_position;    // 0-9: where the click releases
        uint8_t  strength;        // 1-8: resistance level (DSX scale, = internal+1)
        uint16_t position_mask;   // raw 10-bit field; exactly two bits set
        const uint8_t* raw_params;
    };

    // Decoded view of a Slope Feedback effect.
    // IMPORTANT: DSX encodes slope as DS_TRIGGER_EFFECT_FEEDBACK (0x21), NOT 0x22.
    // The only wire-level difference from uniform Feedback is that per-position
    // strengths vary linearly from start_strength (at start_position) to end_strength
    // (at end_position), with all positions beyond end_position holding end_strength.
    // Wire encoding is identical to FeedbackTriggerEffect.
    //
    // end_position detection caveat: rounding of the linear interpolation can cause
    // two adjacent positions to share the same strength value at the end of the slope,
    // shifting the detected end_position one earlier than the DSX UI value in some cases.
    // When start_strength == end_strength (flat), end_position is set to the last
    // active position as a best-effort approximation.
    struct SlopeFeedbackEffect {
        uint8_t  start_position;            // lowest active position (0-9)
        uint8_t  end_position;              // best-effort detected slope end position
        uint8_t  start_strength;            // resistance at start_position (1-8 DSX scale)
        uint8_t  end_strength;              // resistance at last active position (1-8 DSX scale)
        uint16_t position_mask;             // raw 10-bit field; bit N = position N is active
        uint8_t  per_position_strength[10]; // 0 = inactive; 1-8 = resistance at that position
        const uint8_t* raw_params;

        // True when all active positions share the same strength (degenerate flat feedback).
        bool isFlat() const {
            for (int i = 0; i < 10; i++)
                if (per_position_strength[i] && per_position_strength[i] != start_strength)
                    return false;
            return true;
        }
    };

    // Decoded view of a Multiple Position Feedback effect.
    // Wire encoding is identical to FeedbackTriggerEffect (same mode 0x21), but the
    // bitmask in params[0..1] has a non-contiguous pattern — any combination of the
    // 10 position bits may be set, with each position independently configurable.
    // Distinguish from Feedback/Slope by checking bitmask contiguity: Feedback/Slope
    // always have all bits from start_position to bit 9 set; MPF does not.
    struct MultiPositionFeedbackEffect {
        uint16_t position_mask;              // raw 10-bit field; any bits may be set
        uint8_t  per_position_strength[10]; // 0 = inactive; 1-8 = resistance (DSX scale)
        const uint8_t* raw_params;
    };

    // Decoded view of a Multiple Position Vibration effect.
    // Wire encoding:
    //   mode         : 0x26 — same wire mode as single Vibration.
    //   params[0..1] : 10-bit LE bitmask — bit N = region N active (any combination).
    //                  Can be contiguous (all regions from start to 9 active) — bitmask
    //                  contiguity alone does NOT distinguish MPV from single Vibration.
    //   params[2..5] : 30-bit packed field, 3 bits per position.
    //                  amplitude[pos] = (packed32 >> (pos*3)) & 7  (0-7 internal).
    //                  DSX "Amplitude N" maps to internal value N-1.
    //   params[6..7] : unused / zero.
    //   params[8]    : frequency in Hz (RELIABLE distinguishing factor from single Vibration).
    //   params[9]    : always zero (single Vibration puts frequency here instead).
    //
    // To distinguish from single Vibration: check params[9].
    //   params[9] != 0  →  single Vibration (frequency = params[9], params[8] = 0)
    //   params[9] == 0  →  MPV             (frequency = params[8], params[9] = 0)
    struct MultiPositionVibrationEffect {
        uint16_t position_mask;               // raw 10-bit field; any bits may be set
        uint8_t  per_position_amplitude[10];  // 0 = inactive; 1-8 = amplitude (DSX scale)
        uint8_t  frequency;                   // Hz (from params[8], not params[9])
        const uint8_t* raw_params;
    };

    // Decoded view of a Vibration effect (DS_TRIGGER_EFFECT_VIBRATION, 0x26).
    // Wire encoding:
    //   params[0..1] : 10-bit LE bitmask — bit N = position N active.
    //                  Positions >= start_position are set (identical to Feedback).
    //   params[2..5] : 30-bit packed field, 3 bits per position (identical to Feedback).
    //                  amplitude[pos] = (packed32 >> (pos*3)) & 7  (0-7 internal).
    //                  DSX "Amplitude N" maps to internal value N-1.
    //   params[6..8] : params[6]=0, params[7]=0, params[8]=0 — p[8]=0 distinguishes this
    //                  from MultiPositionVibrationEffect which puts frequency at p[8].
    //   params[9]    : frequency in Hz — p[9]!=0 is the reliable single-Vibration marker.
    //   (11 bytes total; params[0..9] = 10 bytes after the mode byte)
    struct VibrationTriggerEffect {
        uint8_t  start_position;             // lowest active position in the bitmask (0-9)
        uint16_t position_mask;              // raw 10-bit field; bit N = position N is active
        uint8_t  per_position_amplitude[10]; // 0 = inactive; 1-8 = amplitude at that position
        uint8_t  frequency;                  // Hz (direct raw value from params[9])
        const uint8_t* raw_params;

        // Amplitude of the first active position (0 if none).
        // In the common single-amplitude case (DSX) this is the uniform level.
        uint8_t amplitude() const {
            for (int i = 0; i < 10; i++)
                if (per_position_amplitude[i]) return per_position_amplitude[i];
            return 0;
        }
    };

    // Generic accessor returned by leftTrigger() / rightTrigger().
    // Subtype is classified eagerly when the accessor is called; subtype() is
    // a one-load read of resolved_subtype. For mode-specific decoded views,
    // call the matching as*() helper below.
    struct ParsedTriggerEffect {
        uint8_t                mode;             // raw wire byte; compare against DsTriggerMode
        const uint8_t*         raw_params;       // points into the 10-byte param array
        DsTriggerEffectSubtype resolved_subtype; // populated by classifySubtype()

        // Decode as a Feedback effect. Only call when mode == DS_TRIGGER_EFFECT_FEEDBACK.
        FeedbackTriggerEffect asFeedback() const {
            FeedbackTriggerEffect fb;
            fb.raw_params = raw_params;

            // 10-bit position bitmask from params[0..1]
            fb.position_mask = (uint16_t)raw_params[0] | ((uint16_t)raw_params[1] << 8);
            fb.position_mask &= 0x03FF;

            // start_position = lowest set bit
            fb.start_position = 0;
            for (int i = 0; i < 10; i++) {
                if (fb.position_mask & (1 << i)) { fb.start_position = (uint8_t)i; break; }
            }

            // 30-bit packed strengths from params[2..5], 3 bits per position
            uint32_t packed = (uint32_t)raw_params[2]
                            | ((uint32_t)raw_params[3] << 8)
                            | ((uint32_t)raw_params[4] << 16)
                            | ((uint32_t)raw_params[5] << 24);

            for (int i = 0; i < 10; i++) {
                if (fb.position_mask & (1 << i))
                    fb.per_position_strength[i] = (uint8_t)(((packed >> (i * 3)) & 0x7) + 1);
                else
                    fb.per_position_strength[i] = 0;
            }

            return fb;
        }

        // Decode as a Weapon effect. Only call when mode == DS_TRIGGER_EFFECT_WEAPON.
        WeaponTriggerEffect asWeapon() const {
            WeaponTriggerEffect w;
            w.raw_params = raw_params;

            // 10-bit position bitmask from params[0..1]; exactly two bits set
            w.position_mask = (uint16_t)raw_params[0] | ((uint16_t)raw_params[1] << 8);
            w.position_mask &= 0x03FF;

            // start = lowest set bit, end = highest set bit
            w.start_position = 0;
            for (int i = 0; i < 10; i++) {
                if (w.position_mask & (1 << i)) { w.start_position = (uint8_t)i; break; }
            }
            w.end_position = 0;
            for (int i = 9; i >= 0; i--) {
                if (w.position_mask & (1 << i)) { w.end_position = (uint8_t)i; break; }
            }

            // params[2]: internal 0-7 → DSX 1-8
            w.strength = raw_params[2] + 1;

            return w;
        }

        // Decode as a Slope Feedback effect. Call when mode == DS_TRIGGER_EFFECT_FEEDBACK (0x21)
        // and the per-position strengths are known to vary (i.e., DSX Slope preset).
        // Also works for uniform Feedback — isFlat() will return true in that case.
        SlopeFeedbackEffect asSlope() const {
            SlopeFeedbackEffect s;
            s.raw_params = raw_params;

            // 10-bit position bitmask from params[0..1]
            s.position_mask = (uint16_t)raw_params[0] | ((uint16_t)raw_params[1] << 8);
            s.position_mask &= 0x03FF;

            // Locate first and last active positions
            s.start_position = 0;
            uint8_t last_active = 0;
            for (int i = 0; i < 10; i++)
                if (s.position_mask & (1 << i)) { s.start_position = (uint8_t)i; break; }
            for (int i = 9; i >= 0; i--)
                if (s.position_mask & (1 << i)) { last_active = (uint8_t)i; break; }

            // 30-bit packed strengths from params[2..5], 3 bits per position
            uint32_t packed = (uint32_t)raw_params[2]
                            | ((uint32_t)raw_params[3] << 8)
                            | ((uint32_t)raw_params[4] << 16)
                            | ((uint32_t)raw_params[5] << 24);

            for (int i = 0; i < 10; i++) {
                if (s.position_mask & (1 << i))
                    s.per_position_strength[i] = (uint8_t)(((packed >> (i * 3)) & 0x7) + 1);
                else
                    s.per_position_strength[i] = 0;
            }

            s.start_strength = s.per_position_strength[s.start_position];
            s.end_strength   = s.per_position_strength[last_active];

            // Detect end_position: scan backward through the flat tail at end_strength.
            // end_position = last position where strength still differs from its successor.
            // Falls back to last_active when the effect is flat (start == end strength).
            s.end_position = last_active;
            bool found_change = false;
            for (int i = (int)last_active - 1; i >= (int)s.start_position; i--) {
                if (s.per_position_strength[i + 1] != s.per_position_strength[i]) {
                    found_change = true;
                    break;
                }
                s.end_position = (uint8_t)i;
            }
            if (!found_change) s.end_position = last_active;

            return s;
        }

        // Decode as a Vibration effect. Only call when mode == DS_TRIGGER_EFFECT_VIBRATION.
        // Encoding is identical to Feedback for the position mask (params[0..1]) and
        // per-position amplitude (params[2..5], 3 bits each, internal+1 = DSX scale).
        // params[9] carries frequency in Hz directly.
        VibrationTriggerEffect asVibration() const {
            VibrationTriggerEffect v;
            v.raw_params = raw_params;

            // 10-bit position bitmask from params[0..1]
            v.position_mask = (uint16_t)raw_params[0] | ((uint16_t)raw_params[1] << 8);
            v.position_mask &= 0x03FF;

            // start_position = lowest set bit
            v.start_position = 0;
            for (int i = 0; i < 10; i++) {
                if (v.position_mask & (1 << i)) { v.start_position = (uint8_t)i; break; }
            }

            // 30-bit packed amplitudes from params[2..5], 3 bits per position
            uint32_t packed = (uint32_t)raw_params[2]
                            | ((uint32_t)raw_params[3] << 8)
                            | ((uint32_t)raw_params[4] << 16)
                            | ((uint32_t)raw_params[5] << 24);

            for (int i = 0; i < 10; i++) {
                if (v.position_mask & (1 << i))
                    v.per_position_amplitude[i] = (uint8_t)(((packed >> (i * 3)) & 0x7) + 1);
                else
                    v.per_position_amplitude[i] = 0;
            }

            // params[9]: frequency in Hz (direct value)
            v.frequency = raw_params[9];

            return v;
        }

        // Decode as a Multiple Position Feedback effect.
        // Call when mode == DS_TRIGGER_EFFECT_FEEDBACK (0x21) and the bitmask is
        // non-contiguous (i.e., not all bits from start_position to bit 9 are set).
        // Wire encoding is identical to asFeedback().
        MultiPositionFeedbackEffect asMultiPosition() const {
            MultiPositionFeedbackEffect mp;
            mp.raw_params = raw_params;

            mp.position_mask = (uint16_t)raw_params[0] | ((uint16_t)raw_params[1] << 8);
            mp.position_mask &= 0x03FF;

            uint32_t packed = (uint32_t)raw_params[2]
                            | ((uint32_t)raw_params[3] << 8)
                            | ((uint32_t)raw_params[4] << 16)
                            | ((uint32_t)raw_params[5] << 24);

            for (int i = 0; i < 10; i++) {
                if (mp.position_mask & (1 << i))
                    mp.per_position_strength[i] = (uint8_t)(((packed >> (i * 3)) & 0x7) + 1);
                else
                    mp.per_position_strength[i] = 0;
            }

            return mp;
        }

        // Decode as a Multiple Position Vibration effect.
        // Call when mode == DS_TRIGGER_EFFECT_VIBRATION (0x26) and the bitmask is
        // non-contiguous (i.e., not all bits from start_position to bit 9 are set).
        // Note: frequency is read from params[8], NOT params[9] as in asVibration().
        MultiPositionVibrationEffect asMultiVibration() const {
            MultiPositionVibrationEffect mv;
            mv.raw_params = raw_params;

            mv.position_mask = (uint16_t)raw_params[0] | ((uint16_t)raw_params[1] << 8);
            mv.position_mask &= 0x03FF;

            uint32_t packed = (uint32_t)raw_params[2]
                            | ((uint32_t)raw_params[3] << 8)
                            | ((uint32_t)raw_params[4] << 16)
                            | ((uint32_t)raw_params[5] << 24);

            for (int i = 0; i < 10; i++) {
                if (mv.position_mask & (1 << i))
                    mv.per_position_amplitude[i] = (uint8_t)(((packed >> (i * 3)) & 0x7) + 1);
                else
                    mv.per_position_amplitude[i] = 0;
            }

            mv.frequency = raw_params[8];

            return mv;
        }

        // Classified sub-effect. Populated eagerly by classifySubtype() when this
        // struct is built by leftTrigger() / rightTrigger(); subtype() is a cheap
        // one-load read.
        DsTriggerEffectSubtype subtype() const { return resolved_subtype; }

        // Classify a raw (mode, params) pair into a DsTriggerEffectSubtype.
        //
        // DsTriggerMode::Feedback (0x21) is disambiguated by position-mask contiguity:
        //   contiguous + uniform strengths  → DS_TRIGGER_SUBTYPE_FEEDBACK
        //   contiguous + varying strengths  → DS_TRIGGER_SUBTYPE_SLOPE_FEEDBACK
        //   non-contiguous                  → DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_FEEDBACK
        //
        // DsTriggerMode::Vibration (0x26) is disambiguated by frequency byte position:
        //   params[9] != 0  → DS_TRIGGER_SUBTYPE_VIBRATION            (freq at params[9])
        //   params[9] == 0  → DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_VIBRATION (freq at params[8])
        static DsTriggerEffectSubtype classifySubtype(uint8_t mode, const uint8_t* params) {
            // Two "off" encodings exist in the wild: native DSX/firmware sends
            // 0x05 (DsTriggerMode::Off); DSX's "Dualsense emulation" mode sends a
            // fully-zeroed trigger block (mode 0x00 + zero params) while still
            // raising the valid-flag bits. Treat the zero block as off too, else
            // it falls through to UNKNOWN and logs as unknown(0x0,...). Verified
            // by raw capture: both forms ride an identical 141-byte 0x31 report.
            if (mode == 0x00)
                return DS_TRIGGER_SUBTYPE_OFF;

            switch (static_cast<DsTriggerMode>(mode)) {
                case DsTriggerMode::Off:
                    return DS_TRIGGER_SUBTYPE_OFF;

                case DsTriggerMode::Feedback: {
                    uint16_t mask = (uint16_t)params[0] | ((uint16_t)params[1] << 8);
                    mask &= 0x03FF;

                    int bstart = -1;
                    for (int i = 0; i < 10; i++) {
                        if (mask & (1 << i)) { bstart = i; break; }
                    }
                    bool contiguous = (bstart < 0);
                    if (bstart >= 0) {
                        uint16_t expected = 0;
                        for (int i = bstart; i < 10; i++) expected |= (uint16_t)(1 << i);
                        contiguous = (mask == expected);
                    }

                    if (!contiguous)
                        return DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_FEEDBACK;

                    uint32_t packed = (uint32_t)params[2]
                                    | ((uint32_t)params[3] << 8)
                                    | ((uint32_t)params[4] << 16)
                                    | ((uint32_t)params[5] << 24);
                    uint8_t first = 0;
                    bool found = false, flat = true;
                    for (int i = 0; i < 10 && flat; i++) {
                        if (!(mask & (1 << i))) continue;
                        uint8_t s = (uint8_t)(((packed >> (i * 3)) & 0x7) + 1);
                        if (!found) { first = s; found = true; }
                        else if (s != first) flat = false;
                    }
                    return flat ? DS_TRIGGER_SUBTYPE_FEEDBACK : DS_TRIGGER_SUBTYPE_SLOPE_FEEDBACK;
                }

                case DsTriggerMode::Weapon:
                    return DS_TRIGGER_SUBTYPE_WEAPON;

                case DsTriggerMode::Vibration:
                    return (params[9] != 0)
                        ? DS_TRIGGER_SUBTYPE_VIBRATION
                        : DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_VIBRATION;

                default:
                    return DS_TRIGGER_SUBTYPE_UNKNOWN;
            }
        }
    };
    ParsedTriggerEffect leftTrigger() const {
        return { left_trigger_motor_mode, left_trigger_param,
                 ParsedTriggerEffect::classifySubtype(left_trigger_motor_mode, left_trigger_param) };
    }
    ParsedTriggerEffect rightTrigger() const {
        return { right_trigger_motor_mode, right_trigger_param,
                 ParsedTriggerEffect::classifySubtype(right_trigger_motor_mode, right_trigger_param) };
    }

    // Haptic-audio window: bytes immediately after the 47-byte common section
    // and before the 4-byte CRC trailer. On the standard 78-byte BT 0x31
    // wire form (or 77-byte BLE form with report_id stripped) this yields
    // 24 bytes = 12 stereo 8-bit-signed PCM frames. See HapticAudioFrame.
    static constexpr size_t DS_HAPTIC_WINDOW_BYTES   = 24;
    static constexpr size_t DS_HAPTIC_FRAMES_PER_PKT = DS_HAPTIC_WINDOW_BYTES / 2;
    static constexpr size_t DS_COMMON_SECTION_BYTES  = 47;
    static constexpr size_t DS_CRC_BYTES             = 4;

    bool hasHapticAudio() const {
        if (!raw_data) return false;
        // Report 0x32 / 0x36: haptic samples live in the parsed 0x12 sub-packet,
        // not in a fixed window. Both formats use the same sub-packet framing.
        if (report_id == 0x32 || report_id == 0x36) {
            return haptic_pkt12_data != nullptr && haptic_pkt12_length >= 2;
        }
        // Report 0x31 / 0x02: 24-byte window after the 47-byte common section.
        const size_t need = (size_t)common_offset_used
                          + DS_COMMON_SECTION_BYTES
                          + DS_HAPTIC_WINDOW_BYTES;
        // CRC is optional (firmware tolerates short reports); window must fit
        // even when no CRC is present.
        return raw_size >= need;
    }

    HapticAudioFrame hapticAudio() const {
        HapticAudioFrame f;
        if (!hasHapticAudio()) return f;

        if (report_id == 0x32 || report_id == 0x36) {
            // Sub-packet 0x12 carries int8 stereo samples; sampleCount is the
            // per-channel frame count (32 stereo frames for the canonical
            // 64-byte payload, but variable in principle). Same framing for
            // both 0x32 (142-byte buffer) and 0x36 (398-byte buffer).
            f.samples     = reinterpret_cast<const int8_t*>(haptic_pkt12_data);
            f.sampleCount = haptic_pkt12_length / 2;
            f.seq         = (seq_tag >> 4) & 0x0F;
            // audio_control / audio_control2 don't apply here — leave at 0.
            if (haptic_pkt11_data && haptic_pkt11_length >= 7) {
                f.controlEnable  = haptic_pkt11_data[0];
                f.controlCounter = haptic_pkt11_data[6];
            }
            return f;
        }

        // 0x31 / 0x02 path — fixed 24-byte window after the common section.
        const size_t window_start = (size_t)common_offset_used + DS_COMMON_SECTION_BYTES;
        f.samples           = reinterpret_cast<const int8_t*>(raw_data + window_start);
        f.sampleCount       = DS_HAPTIC_FRAMES_PER_PKT;
        f.audioControlBits  = audio_control;
        f.audioControl2Bits = audio_control2;
        f.seq               = (seq_tag >> 4) & 0x0F;
        return f;
    }

    // ----- Named predicate accessors --------------------------------------
    // Wrap the raw valid_flag* bit checks so user code reads as intent rather
    // than as bit arithmetic. The raw flag bytes and DS_OUT_FLAG*_* macros
    // remain public for advanced use.

    // True when the host intends the motor bytes to be applied.
    // The explicit-flag path covers standard DualSense output reports.
    // The zero-flag fallback covers the motor-stop reports that Steam sends
    // with neither flag set (empirically, Steam clears both flags when
    // sending motors=0 to stop rumble, even though it sets them for motor-on).
    bool hasRumble() const {
        return (valid_flag0 & DS_OUT_FLAG0_COMPATIBLE_VIBRATION)
            || (valid_flag2 & DS_OUT_FLAG2_COMPATIBLE_VIBRATION2)
            || (valid_flag0 == 0 && valid_flag2 == 0);
    }

    // Adaptive triggers — true when the host populated the corresponding
    // 11-byte trigger effect block (mode + 10 params). Use leftTrigger() /
    // rightTrigger() to decode.
    bool hasLeftTriggerEffect()  const { return valid_flag0 & DS_OUT_FLAG0_LEFT_TRIGGER_EFFECT;  }
    bool hasRightTriggerEffect() const { return valid_flag0 & DS_OUT_FLAG0_RIGHT_TRIGGER_EFFECT; }

    // LEDs / lightbar — DSX often sets LIGHTBAR_SETUP without LIGHTBAR;
    // hasLightbar() reports either path.
    bool hasLightbar() const {
        return (valid_flag1 & DS_OUT_FLAG1_LIGHTBAR)
            || (valid_flag2 & DS_OUT_FLAG2_LIGHTBAR_SETUP);
    }
    bool hasPlayerIndicator() const { return valid_flag1 & DS_OUT_FLAG1_PLAYER_INDICATOR; }
    bool hasMicMuteLed()      const { return valid_flag1 & DS_OUT_FLAG1_MIC_MUTE_LED;     }

    // Convenience aliases mirroring the canonical wire roles. motor_right is
    // the weak (high-frequency) motor; motor_left is the strong (low-
    // frequency) motor.
    uint8_t weakMotor()   const { return motor_right;  }
    uint8_t strongMotor() const { return motor_left; }

    // default constructor OK
    DualsenseGamepadOutputReportData() = default;

    // Walk the report-0x32 sub-packet stream starting at byte 2 (after
    // report_id and seq_tag). Stops at end-of-buffer minus 4 (CRC trailer)
    // or at the first malformed packet. Bounds-checked: a truncated or
    // mismatched-length report cannot OOB-read.
    //
    // Sub-packet header byte: pid (low 6 bits) | unk (bit 6) | sized (bit 7).
    // We only handle sized packets (length byte follows the header).
    void parseReport0x32SubPackets(const uint8_t* value, size_t size)
    {
        haptic_pkt12_data   = nullptr;
        haptic_pkt12_length = 0;
        haptic_pkt11_data   = nullptr;
        haptic_pkt11_length = 0;
        if (size < 2 + 4) return;
        const size_t end = size - 4;  // leave room for CRC32 trailer
        size_t off = 2;
        while (off + 1 < end) {
            uint8_t header = value[off++];
            uint8_t pid    = header & 0x3F;
            bool    sized  = (header >> 7) & 0x01;
            if (!sized) break;  // unsized packets unsupported; abort walk
            uint8_t length = value[off++];
            if (off + length > end) break;  // truncated: stop walking
            const uint8_t* data = value + off;
            if (pid == 0x12) {
                haptic_pkt12_data   = data;
                haptic_pkt12_length = length;
            } else if (pid == 0x11) {
                haptic_pkt11_data   = data;
                haptic_pkt11_length = length;
            }
            off += length;
        }
    }

    // parsing function - reads from raw BLE output report bytes
    // Handles multiple formats:
    // - 398 bytes: report 0x36 (haptic-audio sub-protocol) - recent Unreal-Dualsense
    // - 142 bytes: report 0x32 (haptic-audio sub-protocol) - SAxense / soundsense
    // - 141 bytes: extended DSX-over-BLE report (3-byte header + common, offset 3)
    // - 78 bytes: Full BLE report (report_id=0x31 + seq_tag + tag + common)
    // - 77 bytes: USB-over-BLE (seq + common) - older DSX sends this format
    //            OR BLE report with report_id stripped (seq_tag + tag + common)
    // - 63 bytes: USB report (report_id=0x02 + common)
    // - 62 bytes: USB report with report_id stripped (common only)
    //
    // DSX 77-byte format:  [seq 0x00-0x0F] [valid_flag0] [valid_flag1] [motor_r] [motor_l] ...
    // DSX 141-byte format: [seq] [..] [tag] [valid_flag0] [valid_flag1] [motor_r] [motor_l] ...
    bool load(const uint8_t* value, size_t size)
    {
        if (!value) return false;

        // Report 0x32 / 0x36 (haptic-audio sub-protocol). No common section —
        // bail out after parsing sub-packets so the rest of this function
        // doesn't try to interpret the bytes as common-format fields.
        // 0x32 is the original 142-byte variant (SAxense / soundsense / older
        // Unreal-Dualsense). 0x36 is the larger 398-byte variant emitted by
        // recent Unreal-Dualsense builds; it uses the same 0x11/0x12 sub-packet
        // framing in a wider buffer, so the same walker handles both.
        if (size >= 12 && (value[0] == 0x32 || value[0] == 0x36)) {
            report_id          = value[0];
            seq_tag            = value[1];
            tag                = 0;
            raw_data           = value;
            raw_size           = size;
            common_offset_used = -1;  // sentinel: no common section
            parseReport0x32SubPackets(value, size);
            return true;
        }

        if (size < 47)  // Minimum: common section is 47 bytes
            return false;

        int common_offset = 0;  // Offset to start of common section

        if (size >= 77) {
            // 77-78 byte reports - could be BLE or USB-over-BLE format
            if (size >= 78 && value[0] == 0x31) {
                // Full BLE report with report_id 0x31
                report_id = value[0];
                seq_tag = value[1];
                tag = value[2];
                common_offset = 3;
            } else if (value[0] == 0x31) {
                // 77-byte BLE report (report_id present but one byte short)
                report_id = value[0];
                seq_tag = value[1];
                tag = value[2];
                common_offset = 3;
            } else if (size >= 79) {
                // Extended DSX-over-BLE report (empirically 141 bytes on current
                // DSX/Windows). Larger than the 78-byte standard BLE report and
                // not a 0x32/0x36 haptic report (those return early above). The
                // layout is a 3-byte header [seq][..][tag] followed by the
                // standard 47-byte common section: lightbar RGB, trigger-effect,
                // and valid-flag fields only line up at offset 3 (verified by raw
                // capture — RGB at value[47..49], trigger modes 0x05 at value
                // [13]/[24]). The smaller USB-over-BLE form (<=78 B) keeps the
                // offset-1 handling below. Must precede the value[0]<=0x0F check
                // because this report's seq byte is sometimes small (e.g. 0x00).
                report_id = 0x31;   // logically a 0x31-family report
                seq_tag = value[0];
                tag = value[2];
                common_offset = 3;
            } else if (value[1] == 0x10) {
                // BLE report with report_id stripped, tag=0x10 at byte 1
                report_id = 0x31;  // Implied
                seq_tag = value[0];
                tag = value[1];
                common_offset = 2;
            } else if (value[0] <= 0x0F) {
                // USB-over-BLE format: first byte is seq counter (0x00-0x0F)
                // Common section starts at byte 1
                // Format: [seq] [valid_flag0] [valid_flag1] [motor_r] [motor_l] ...
                report_id = 0x02;  // USB format
                seq_tag = value[0];
                tag = 0;
                common_offset = 1;
            } else {
                // Unknown format - try assuming common starts at byte 1
                // This handles DSX sending USB-style reports over BLE
                report_id = 0x02;
                seq_tag = value[0];
                tag = 0;
                common_offset = 1;
            }
        } else if (size >= 62) {
            // USB format (62-63 bytes)
            if (size >= 63 && value[0] == 0x02) {
                // Full USB report with report_id
                report_id = value[0];
                seq_tag = 0;
                tag = 0;
                common_offset = 1;
            } else {
                // USB report with report_id stripped
                report_id = 0x02;
                seq_tag = 0;
                tag = 0;
                common_offset = 0;
            }
        } else {
            // Too small - try parsing as common section directly
            report_id = 0;
            seq_tag = 0;
            tag = 0;
            common_offset = 0;
        }

        // Stash the raw buffer for downstream consumers that need to inspect
        // bytes outside the named fields (haptic-audio sniffer, hapticAudio()).
        // Borrowed pointer; valid only for the lifetime of this load() call's
        // caller scope (i.e., the onReceivedOutputReport callback).
        raw_data = value;
        raw_size = size;
        common_offset_used = common_offset;

        // Parse common section (47 bytes)
        // Common structure: valid_flag0, valid_flag1, motor_right, motor_left, ...
        valid_flag0 = value[common_offset + 0];
        valid_flag1 = value[common_offset + 1];
        motor_right = value[common_offset + 2];
        motor_left = value[common_offset + 3];

        headphone_volume = value[common_offset + 4];
        speaker_volume = value[common_offset + 5];
        mic_volume = value[common_offset + 6];
        audio_control = value[common_offset + 7];
        mute_button_led = value[common_offset + 8];
        power_save_control = value[common_offset + 9];

        // Adaptive-trigger blocks at common+10 through common+36.
        right_trigger_motor_mode = value[common_offset + 10];
        for (int i = 0; i < 10; ++i) right_trigger_param[i] = value[common_offset + 11 + i];
        left_trigger_motor_mode = value[common_offset + 21];
        for (int i = 0; i < 10; ++i) left_trigger_param[i]  = value[common_offset + 22 + i];
        for (int i = 0; i < 4;  ++i) reserved_host_timestamp[i] = value[common_offset + 32 + i];
        reduce_motor_power = value[common_offset + 36];

        audio_control2 = value[common_offset + 37];

        valid_flag2 = value[common_offset + 38];
        // reserved3[2] at common+39, common+40
        lightbar_setup = value[common_offset + 41];
        led_brightness = value[common_offset + 42];
        player_leds = value[common_offset + 43];
        lightbar_red = value[common_offset + 44];
        lightbar_green = value[common_offset + 45];
        lightbar_blue = value[common_offset + 46];

        // CRC is at the end of the report (if present)
        if (size >= common_offset + 47 + 4) {
            size_t crc_offset = size - 4;
            crc32 = (uint32_t)value[crc_offset]
                | ((uint32_t)value[crc_offset + 1] << 8)
                | ((uint32_t)value[crc_offset + 2] << 16)
                | ((uint32_t)value[crc_offset + 3] << 24);
        } else {
            crc32 = 0;
        }

        return true;
    }
};

// DualSense BLE Input Report structure (77 bytes, sent after report ID 0x31 prepended by HID layer)
// Byte layout matches `struct dualsense_input_report` in Linux hid-playstation.c
// with a leading BT header byte (bit 0 = HasHID, must be 1; bits 4-7 = seq).
#pragma pack(push, 1)
struct DualsenseGamepadInputReportData {
    uint8_t bt = 0x01;              // byte  0: BLE header (HasHID=1)
    uint8_t x  = 0x80;              // byte  1: left stick X  (ABS_X,  0x80 = centered)
    uint8_t y  = 0x80;              // byte  2: left stick Y  (ABS_Y)
    uint8_t rx = 0x80;              // byte  3: right stick X (ABS_RX, 0x80 = centered)
    uint8_t ry = 0x80;              // byte  4: right stick Y (ABS_RY)
    uint8_t z  = 0;                 // byte  5: L2 analog trigger (ABS_Z)
    uint8_t rz = 0;                 // byte  6: R2 analog trigger (ABS_RZ)
    uint8_t seq = 0x20;             // byte  7: sequence number
    // bytes 8-10: hat (low nibble of byte 8) + 20 buttons spread across the high nibble of
    // byte 8 and all of bytes 9-10. `uint8_t buttons[3]` makes the byte layout deterministic
    // across toolchains (mixing uint8_t/uint32_t bitfields under __attribute__((packed)) is
    // compiler-dependent).
    uint8_t buttons[3] = { 0x08, 0, 0 };  // byte 8 low nibble hat defaults to 0x08 (NONE)
    uint8_t extra_buttons = 0;      // byte 11: Edge paddles / misc
    uint32_t reserved = 0;          // bytes 12-15
    int16_t gyro_x = 0;             // bytes 16-17
    int16_t gyro_y = 0;             // bytes 18-19
    int16_t gyro_z = 0;             // bytes 20-21
    int16_t accel_x = 0;            // bytes 22-23
    int16_t accel_y = 0;            // bytes 24-25
    int16_t accel_z = 0;            // bytes 26-27
    uint32_t timestamp = 0x7621DD40;// bytes 28-31: sensor timestamp
    uint8_t reserved2 = 0;          // byte 32
    uint8_t touchpoint_l_contact = 0x80;  // byte 33 (0x80 = no contact)
    uint16_t touchpoint_l_x : 12;   // bytes 34-35 (low 12 bits across two bytes, packed with y)
    uint16_t touchpoint_l_y : 12;   // byte 36 (high 12 bits)
    uint8_t touchpoint_r_contact = 0x80;  // byte 37
    uint16_t touchpoint_r_x : 12;   // bytes 38-39
    uint16_t touchpoint_r_y : 12;   // byte 40
    uint8_t data_41_48[8];          // bytes 41-48: reserved
    // byte 49 (BT) / 48 (USB): DualSense Edge active-profile state byte.
    // dualsense-tester (daidr/dualsense-tester) treats the controller as being
    // in "configuration mode" — disabling Output/Audio/Gyro/Accel panels — when
    // this byte is zero or its low two bits are set. Default 0x10 = profile
    // slot 1 active with config-menu bits clear.
    uint8_t active_profile = 0x10;  // byte 49
    uint8_t data_50_52[3];          // bytes 50-52: reserved

    // byte 53: battery/status[0]. Low nibble = capacity 0-10, high nibble = charging state.
    // 0x0A = 100% discharging (normal). 0xFF would read as capacity=15 + charging=0xF (error).
    uint8_t status = 0x0A;          // byte 53

    uint8_t data_54_74[18];         // bytes 54-71
    uint8_t reserved3 = 0x00;       // byte 72
    uint32_t crc32 = 0;             // bytes 73-76
} __attribute__((packed));

#pragma pack(pop)

static_assert(sizeof(DualsenseGamepadInputReportData) == 77, "Input report must be 77 bytes (78 on wire after report ID prepended)");

struct DualsenseGamepadPairingReportdata {
    uint8_t mac_address[6];
    uint8_t common[9];
    uint32_t crc32;
    // Linux note:
    // BlueZ off-by-one: bt_uhid_get_report_reply delivers (gatt_size) bytes to the kernel
    // rather than (gatt_size+1), so the kernel sees one fewer byte than we sent. Adding a
    // trailing padding byte makes the kernel receive the full [report_id + mac + common + crc32]
    // = 20 bytes it expects. This byte is dropped by BlueZ and never reaches hid-playstation.
    uint8_t _padding;
} __attribute__((packed));

static_assert(sizeof(DualsenseGamepadPairingReportdata) == DUALSENSE_PAIRING_INFO_REPORT_SIZE,
    "DualsenseGamepadPairingReportdata must match DUALSENSE_PAIRING_INFO_REPORT_SIZE and the HID descriptor's Report Count for 0x09");

[[maybe_unused]] static uint8_t dPadDirectionToValue(DualsenseDpadFlags direction)
{
    if (direction == DualsenseDpadFlags::NORTH)
        return DUALSENSE_BUTTON_DPAD_NORTH;
    else if (direction == (DualsenseDpadFlags::EAST | DualsenseDpadFlags::NORTH))
        return DUALSENSE_BUTTON_DPAD_NORTHEAST;
    else if (direction == DualsenseDpadFlags::EAST)
        return DUALSENSE_BUTTON_DPAD_EAST;
    else if (direction == (DualsenseDpadFlags::EAST | DualsenseDpadFlags::SOUTH))
        return DUALSENSE_BUTTON_DPAD_SOUTHEAST;
    else if (direction == DualsenseDpadFlags::SOUTH)
        return DUALSENSE_BUTTON_DPAD_SOUTH;
    else if (direction == (DualsenseDpadFlags::WEST | DualsenseDpadFlags::SOUTH))
        return DUALSENSE_BUTTON_DPAD_SOUTHWEST;
    else if (direction == DualsenseDpadFlags::WEST)
        return DUALSENSE_BUTTON_DPAD_WEST;
    else if (direction == (DualsenseDpadFlags::WEST | DualsenseDpadFlags::NORTH))
        return DUALSENSE_BUTTON_DPAD_NORTHWEST;

    return DUALSENSE_BUTTON_DPAD_NONE;
}

[[maybe_unused]] static String dPadDirectionName(uint8_t direction)
{
    if (direction == DUALSENSE_BUTTON_DPAD_NORTH)
        return "NORTH";
    else if (direction == DUALSENSE_BUTTON_DPAD_NORTHEAST)
        return "NORTHEAST";
    else if (direction == DUALSENSE_BUTTON_DPAD_EAST)
        return "EAST";
    else if (direction == DUALSENSE_BUTTON_DPAD_SOUTHEAST)
        return "SOUTHEAST";
    else if (direction == DUALSENSE_BUTTON_DPAD_SOUTH)
        return "SOUTH";
    else if (direction == DUALSENSE_BUTTON_DPAD_SOUTHWEST)
        return "SOUTHWEST";
    else if (direction == DUALSENSE_BUTTON_DPAD_WEST)
        return "WEST";
    else if (direction == DUALSENSE_BUTTON_DPAD_NORTHWEST)
        return "NORTHWEST";
    return "NONE";
}

class DualsenseGamepadDevice : public BaseCompositeDevice {
public:
    DualsenseGamepadDevice();
    DualsenseGamepadDevice(DualsenseGamepadDeviceConfiguration* config);
    ~DualsenseGamepadDevice();

    void init(NimBLEHIDDevice* hid) override;
    const BaseCompositeDeviceConfiguration* getDeviceConfig() const override;

    Signal<DualsenseGamepadOutputReportData> onReceivedOutputReport;

    // Fires once per 0x31 output report that contains a writable haptic-audio
    // window (24 bytes between the common section and the CRC trailer).
    // The HapticAudioFrame holds zero-copy pointers into the live BLE buffer
    // and is valid only for the duration of the slot invocation; copy the
    // samples into a ring buffer if you need them asynchronously.
    // See HapticAudioFrame in this header for the wire-format contract.
    Signal<HapticAudioFrame> onHapticAudioReceived;

    // Input Controls
    void resetInputs();
    void press(uint32_t button = DUALSENSE_BUTTON_A);
    void release(uint32_t button = DUALSENSE_BUTTON_A);
    bool isPressed(uint32_t button = DUALSENSE_BUTTON_A);
    void setLeftThumb(int8_t x = 0, int8_t y = 0);
    void setRightThumb(int8_t x = 0, int8_t y = 0);
    void setLeftTrigger(uint8_t rX = 0);
    void setRightTrigger(uint8_t rY = 0);
    void setTriggers(uint8_t rX = 0, uint8_t rY = 0);
    void pressDPadDirection(uint8_t direction = 0);
    void pressDPadDirectionFlag(DualsenseDpadFlags direction = DualsenseDpadFlags::NONE);
    void releaseDPad();
    bool isDPadPressed(uint8_t direction = 0);
    bool isDPadPressedFlag(DualsenseDpadFlags direction);
    void pressShare();
    void releaseShare();
    void setLeftTouchpad(uint16_t x, uint16_t y);
    void setRightTouchpad(uint16_t x, uint16_t y);
    void releaseLeftTouchpad();
    void releaseRightTouchpad();
    void setAccel(int16_t x, int16_t y, int16_t z);
    void setGyro(int16_t pitch, int16_t yaw, int16_t roll);
    void setBatteryLevel(uint8_t level);
    void setChargingStatus(bool charging);
    void setActiveProfile(uint8_t profile);
    void sendGamepadReport(bool defer = false);
    void sendFirmInfoReport(bool defer = false);
    void sendCalibrationReport(bool defer = false);
    void sendPairingInfoReport(bool defer = false);
    void timestamp();
    void seq();

    // Called by callback when host reads a feature report - populates value before read completes
    void populateFeatureReportOnRead(NimBLECharacteristic* pCharacteristic);

    // Characteristic accessors used by DualsenseGamepadCallbacks to identify
    // which pipe a read/subscribe came from when logging.
    NimBLECharacteristic* getInputChar()          { return getInput(); }
    NimBLECharacteristic* getOutputChar()         { return getOutput(); }
    NimBLECharacteristic* getMinimalInput() const { return _minimalInput; }
    NimBLECharacteristic* getCalibration() const  { return _calibration; }
    NimBLECharacteristic* getFirmwareInfo() const { return _firmwareInfo; }
    NimBLECharacteristic* getPairingInfo() const  { return _pairingInfo; }
    NimBLECharacteristic* getBtPatchInfo() const  { return _btPatchInfo; }

private:
    void sendGamepadReportImpl();
    void sendFirmInfoReportImpl();
    void sendCalibrationReportImpl();
    void sendPairingInfoReportImpl();
    DualsenseGamepadInputReportData _inputReport;
    DualsenseGamepadPairingReportdata _pairingReport;
    void buildFeatureReportWithCrc(uint8_t reportId, const uint8_t* payload,
        size_t payloadSize, uint8_t* outBuffer, size_t outSize);
    DualsenseGamepadDeviceConfiguration* _config;
    NimBLECharacteristic* _extra_input;
    NimBLECharacteristic* _minimalInput;  // 0x01 9-byte Generic Desktop input report (BLE GAP requirement)
    DualsenseGamepadCallbacks* _callbacks;
    NimBLECharacteristic* _calibration;
    NimBLECharacteristic* _firmwareInfo;
    NimBLECharacteristic* _pairingInfo;
    NimBLECharacteristic* _btPatchInfo;
    uint32_t crc32_le(unsigned int crc, unsigned char const* buf, unsigned int len);
    void generate_crc_table(uint32_t* crcTable);
    uint32_t* m_pCrcTable;
    // Threading
    std::mutex _mutex;
};

#endif // DUALSENSE_GAMEPAD_DEVICE_H
