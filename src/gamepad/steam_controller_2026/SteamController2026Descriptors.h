#ifndef STEAM_CONTROLLER_2026_DESCRIPTORS_H
#define STEAM_CONTROLLER_2026_DESCRIPTORS_H

#include <stdint.h>

// Valve VID; BLE PID recognised by Linux hid-steam.c (HID_BLUETOOTH_DEVICE entry)
#define SC2026_VENDOR_ID        0x28DE
#define SC2026_PRODUCT_ID       0x1106
#define SC2026_BCD_DEVICE       0x0001
#define SC2026_SERIAL           "SC2026BLE00000001"

// Report IDs
#define SC2026_INPUT_REPORT_ID  0x01
#define SC2026_OUTPUT_REPORT_ID 0x01
#define SC2026_INPUT_REPORT_SIZE 64   // full 64-byte vendor blob (matches real SC BLE packet size)

// Button bit-positions within the 4-byte button field (LE uint32).
// Layout matches sc_report.h from the SCPlusPLus USB-host reference project.
enum SC2026Button : uint32_t {
    // Byte 0 (bits 0-7)
    SC2026_BTN_A            = 1u <<  0,
    SC2026_BTN_B            = 1u <<  1,
    SC2026_BTN_X            = 1u <<  2,
    SC2026_BTN_Y            = 1u <<  3,
    SC2026_BTN_QUICK_ACCESS = 1u <<  4,
    SC2026_BTN_RSTICK_CLICK = 1u <<  5,
    SC2026_BTN_MENU         = 1u <<  6,
    SC2026_BTN_R4           = 1u <<  7,

    // Byte 1 (bits 8-15)
    SC2026_BTN_R5           = 1u <<  8,
    SC2026_BTN_R1           = 1u <<  9,
    SC2026_BTN_DPAD_DOWN    = 1u << 10,
    SC2026_BTN_DPAD_RIGHT   = 1u << 11,
    SC2026_BTN_DPAD_LEFT    = 1u << 12,
    SC2026_BTN_DPAD_UP      = 1u << 13,
    SC2026_BTN_VIEW         = 1u << 14,
    SC2026_BTN_LSTICK_CLICK = 1u << 15,

    // Byte 2 (bits 16-23)
    SC2026_BTN_STEAM        = 1u << 16,
    SC2026_BTN_L4           = 1u << 17,
    SC2026_BTN_L5           = 1u << 18,
    SC2026_BTN_L1           = 1u << 19,
    SC2026_BTN_RSTICK_CAP   = 1u << 20,  // right stick capacitive touch
    SC2026_BTN_RPAD_TOUCH   = 1u << 21,
    SC2026_BTN_RPAD_CLICK   = 1u << 22,
    SC2026_BTN_R2_DIGITAL   = 1u << 23,

    // Byte 3 (bits 24-31)
    SC2026_BTN_LSTICK_CAP   = 1u << 24,  // left stick capacitive touch
    SC2026_BTN_LPAD_TOUCH   = 1u << 25,
    SC2026_BTN_LPAD_CLICK   = 1u << 26,
    SC2026_BTN_L2_DIGITAL   = 1u << 27,
    SC2026_BTN_RGRIP_CAP    = 1u << 28,  // right capacitive grip sensor
    SC2026_BTN_LGRIP_CAP    = 1u << 29,  // left capacitive grip sensor
};

// Trigger analog range
#define SC2026_TRIGGER_MIN  0
#define SC2026_TRIGGER_MAX  32767

// Stick / touchpad signed range
#define SC2026_AXIS_MIN     (-32768)
#define SC2026_AXIS_MAX     32767

// Minimal vendor-defined HID descriptor.
// hid-steam.c on Linux uses VID/PID matching and parses the raw bytes directly,
// so the descriptor just needs to declare a 64-byte vendor input/output report.
static const uint8_t SC2026_HIDDescriptor[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor 0xFF00)
    0x09, 0x00,        // Usage (0x00)
    0xA1, 0x01,        // Collection (Application)

    // 64-byte input blob - hid-steam.c reads the raw bytes at fixed offsets
    0x85, SC2026_INPUT_REPORT_ID,  //   Report ID
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, SC2026_INPUT_REPORT_SIZE,//   Report Count (64)
    0x09, 0x00,                    //   Usage (0x00)
    0x81, 0x02,                    //   Input (Data, Variable, Absolute)

    // 64-byte output blob (haptic / rumble commands)
    0x85, SC2026_OUTPUT_REPORT_ID, //   Report ID
    0x15, 0x00,                    //   Logical Minimum (0)
    0x26, 0xFF, 0x00,              //   Logical Maximum (255)
    0x75, 0x08,                    //   Report Size (8 bits)
    0x95, SC2026_INPUT_REPORT_SIZE,//   Report Count (64)
    0x09, 0x00,                    //   Usage (0x00)
    0x91, 0x02,                    //   Output (Data, Variable, Absolute)

    0xC0  // End Collection
};

// BLE input report wire layout (64 bytes, Report ID stripped by GATT layer).
// Bytes 0-2 carry the Steam BLE protocol header that hid-steam.c checks;
// the remainder mirrors the USB SCReport payload (without the leading report-ID byte).
struct SteamController2026InputReport {
    // Steam BLE protocol header (3 bytes)
    // hid-steam.c: raw_event checks data[2] == 0x04 (STEAM_INPUT_REPORT_TYPE_CONTROLLER_STATE)
    uint8_t  proto_type    = 0x04;  // byte  0: Steam input event type
    uint8_t  proto_flags   = 0x00;  // byte  1: flags (reserved)
    uint8_t  proto_subtype = 0x04;  // byte  2: 0x04 = controller state

    // Payload matches SCReport layout (bytes 1-53 of USB report, skipping the ID)
    uint8_t  seq          = 0;      // byte  3: rolling sequence counter
    uint8_t  btns[4]      = {0};    // bytes 4-7:  button flags (LE uint32, see SC2026Button)
    uint16_t ltrigger     = 0;      // bytes 8-9:  left trigger  0-32767
    uint16_t rtrigger     = 0;      // bytes 10-11: right trigger 0-32767
    int16_t  left_stick_x = 0;      // bytes 12-13
    int16_t  left_stick_y = 0;      // bytes 14-15
    int16_t  right_stick_x= 0;      // bytes 16-17
    int16_t  right_stick_y= 0;      // bytes 18-19
    int16_t  lpad_x       = 0;      // bytes 20-21: left touchpad X
    int16_t  lpad_y       = 0;      // bytes 22-23: left touchpad Y
    int16_t  lpad_force   = 0;      // bytes 24-25: left touchpad force
    int16_t  rpad_x       = 0;      // bytes 26-27: right touchpad X
    int16_t  rpad_y       = 0;      // bytes 28-29: right touchpad Y
    int16_t  rpad_force   = 0;      // bytes 30-31: right touchpad force
    uint8_t  heartbeat    = 0;      // byte 32
    uint8_t  checksum     = 0;      // byte 33
    uint16_t counter      = 0;      // bytes 34-35: free-running counter
    int16_t  acc_x        = 0;      // bytes 36-37: accelerometer
    int16_t  acc_y        = 0;      // bytes 38-39
    int16_t  acc_z        = 0;      // bytes 40-41
    int16_t  gyro_x       = 0;      // bytes 42-43: gyroscope
    int16_t  gyro_y       = 0;      // bytes 44-45
    int16_t  gyro_z       = 0;      // bytes 46-47
    int16_t  quat_w       = 0;      // bytes 48-49: quaternion
    int16_t  quat_x       = 0;      // bytes 50-51
    int16_t  quat_y       = 0;      // bytes 52-53
    int16_t  quat_z       = 0;      // bytes 54-55
    uint8_t  _pad[8]      = {0};    // bytes 56-63: reserved
};

static_assert(sizeof(SteamController2026InputReport) == SC2026_INPUT_REPORT_SIZE,
    "SteamController2026InputReport must be 64 bytes");

#endif // STEAM_CONTROLLER_2026_DESCRIPTORS_H
