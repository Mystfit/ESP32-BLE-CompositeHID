
#ifndef DUALSENSE_DESCRIPTORS_H
#define DUALSENSE_DESCRIPTORS_H

#include <cstdint>
#include <stdint.h>

#define DUALSENSE_VENDOR_ID 0x054C

// Product: Dualsense Edge
#define DUALSENSE_PRODUCT_ID 0x0CE6

#define DUALSENSE_EDGE_PRODUCT_ID 0x0DF2
#define DUALSENSE_EDGE_BCD_DEVICE_ID 0x0408
#define DUALSENSE_EDGE_SERIAL "G45700GWN10872283"
#define DUALSENSE_EDGE_INPUT_REPORT_ID 0x31
#define DUALSENSE_EDGE_OUTPUT_REPORT_ID 0x31
#define DUALSENSE_EDGE_INPUT_REPORT_BT_SIZE 78
#define DUALSENSE_MINIMAL_INPUT_REPORT_ID 0x01
#define DUALSENSE_MINIMAL_INPUT_REPORT_SIZE 9  // 4 stick bytes + 1 hat+pad byte + 2 buttons+pad bytes + 2 trigger bytes

#define DS_OUTPUT_REPORT_BT_SIZE 78
#define PS_INPUT_CRC32_SEED 0xA1
#define PS_OUTPUT_CRC32_SEED 0xA2
#define PS_FEATURE_CRC32_SEED 0xA3
#define DUALSENSE_CALIBRATION_REPORT_ID 0x05
// Sizes match the HID descriptor's Report Count for each feature report.
// The BLE characteristic value contains payload only (no leading report ID byte).

// Linux note:
// Linux hid-playstation's DS_FEATURE_REPORT_*_SIZE constants include the report ID
// (one byte larger), but BlueZ's bt_uhid_get_report_reply has an off-by-one: it sets
// rsp->size = MIN(gatt_size, max-1) and copies rsp->size-1 bytes of data, so the kernel
// receives (gatt_size) bytes total instead of (gatt_size+1). To compensate, the
// calibration, pairing, and firmware-info characteristics are each one byte larger than
// the kernel's data payload, with a trailing padding byte that BlueZ silently drops.
// This makes the kernel see the expected sizes (41 / 20 / 64 bytes total respectively).
#define DUALSENSE_CALIBRATION_REPORT_SIZE 41
#define DUALSENSE_PAIRING_INFO_REPORT_ID 0x09
#define DUALSENSE_PAIRING_INFO_REPORT_SIZE 20
#define DUALSENSE_FIRMWARE_INFO_REPORT_ID 0x20
#define DUALSENSE_FIRMWARE_INFO_REPORT_SIZE 64
#define DUALSENSE_BT_PATCH_REPORT_ID 0x22
#define DUALSENSE_BT_PATCH_REPORT_SIZE 63

static const uint8_t DualsenseEdge_PairInfo_common[] {
    0x08,
    0x25,
    0x00,
    0x1E,
    0x00,
    0xEE,
    0x74,
    0xD0,
    0xBC
};

static const uint8_t DualsenseEdge_FirmwareInfo[] {
    0x4A,
    0x75,
    0x6E,
    0x20,
    0x31,
    0x39,
    0x20,
    0x32,
    0x30,
    0x32,
    0x33,
    0x31,
    0x34,
    0x3A,
    0x34,
    0x37,
    0x3A,
    0x33,
    0x34,
    0x03,
    0x00,
    0x44,
    0x00,
    0x08,
    0x02,
    0x00,
    0x01,
    0x36,
    0x00,
    0x00,
    0x01,
    0xC1,
    0xC8,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x24, // Version low byte (2.36 = 0x0224) - must be >= 2.21 for vibration v2
    0x02, // Version high byte
    0x00,
    0x00,
    0x14,
    0x00,
    0x00,
    0x00,
    0x0B,
    0x00,
    0x01,
    0x00,
    0x06,
    0x00,
    0x00,
    0x00,
    0x00,
    0x82,
    0x79,
    0xD9,
    0x57
};
static const uint8_t DualsenseEdge_StockCalibration[] {
    0x00, // Gyro Pitch Bias
    0x00,
    0x00, // Gyro Yaw Bias
    0x00,
    0x00, // Gyro Roll Bias
    0x00,
    0x10, // Gyro Pitch Plus
    0x27,
    0xF0, // Gyro Pitch Minus
    0xD8,
    0x10, // Gyro Yaw Plus
    0x27,
    0xF0, // Gyro Yaw Minus
    0xD8,
    0x10, // Gyro Roll Plus
    0x27,
    0xF0, // Gyro Roll Minus
    0xD8,
    0xF4, // Gyro Speed Plus
    0x01,
    0xF4, // Gyro Speed Minus
    0x01,
    0x10, // Accel X Plus
    0x27,
    0xF0, // Accel X Minus
    0xD8,
    0x10, // Accel Y Plus
    0x27,
    0xF0, // Accel Y Minus
    0xD8,
    0x10, // Accel Z Plus
    0x27,
    0xF0, // Accel Z Minus
    0xD8,
    0x0B,
    0x00,
    0x00,
    0x8D,
    0x93,
    0xCA,
    0x2B
};
static const uint8_t DualsenseEdge_HIDDescriptor[] {
    0x05, 0x01, // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05, // Usage (Game Pad)
    0xA1, 0x01, // Collection (Application)

    // ========== Report ID 0x01 (Input) - 9-byte minimal Generic Desktop gamepad ==========
    // Matches the real DualSense BT descriptor (nondebug/dualsense) byte-for-byte.
    // Windows HIDClass requires at least one Generic Desktop Gamepad input report to
    // fully activate the HID stack and enable input-report notifications; without it
    // feature reads work but the input pipe stays dormant. Real DualSense keeps this
    // report alongside the vendor 0x31 blob for the same reason.
    // Usage order on the wire: X, Y, Z, Rz (sticks) / hat+padding / 14 buttons+padding / Rx, Ry (triggers)
    0x85, 0x01, //   Report ID (1)
    0x09, 0x30, //   Usage (X)        -- left stick X
    0x09, 0x31, //   Usage (Y)        -- left stick Y
    0x09, 0x32, //   Usage (Z)        -- right stick X
    0x09, 0x35, //   Usage (Rz)       -- right stick Y
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x04, //   Report Count (4)
    0x81, 0x02, //   Input (Data,Var,Abs)
    0x09, 0x39, //   Usage (Hat switch)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x07, //   Logical Maximum (7)
    0x35, 0x00, //   Physical Minimum (0)
    0x46, 0x3B, 0x01, //   Physical Maximum (315)
    0x65, 0x14, //   Unit (English Rotation: Degrees)
    0x75, 0x04, //   Report Size (4)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x42, //   Input (Data,Var,Abs,Null State)
    0x65, 0x00, //   Unit (None)
    0x05, 0x09, //   Usage Page (Button)
    0x19, 0x01, //   Usage Minimum (0x01)
    0x29, 0x0E, //   Usage Maximum (0x0E)
    0x15, 0x00, //   Logical Minimum (0)
    0x25, 0x01, //   Logical Maximum (1)
    0x75, 0x01, //   Report Size (1)
    0x95, 0x0E, //   Report Count (14)
    0x81, 0x02, //   Input (Data,Var,Abs)          -- 14 digital buttons
    0x75, 0x06, //   Report Size (6)
    0x95, 0x01, //   Report Count (1)
    0x81, 0x01, //   Input (Const)                 -- 6-bit padding to align triggers
    0x05, 0x01, //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x33, //   Usage (Rx)       -- L2 trigger
    0x09, 0x34, //   Usage (Ry)       -- R2 trigger
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x02, //   Report Count (2)
    0x81, 0x02, //   Input (Data,Var,Abs)

    // ========== Report ID 0x31 (Input + Output) - pure vendor-defined 77-byte blob ==========
    // Windows / Sony DualSense driver recognises the device by VID/PID and parses
    // the raw bytes at fixed offsets (matching `struct dualsense_input_report`).
    0x06, 0x00, 0xFF, //   Usage Page (Vendor Defined 0xFF00)
    0x15, 0x00, //   Logical Minimum (0)
    0x26, 0xFF, 0x00, //   Logical Maximum (255)
    0x75, 0x08, //   Report Size (8)
    0x95, 0x4D, //   Report Count (77)
    0x85, 0x31, //   Report ID (49)
    0x09, 0x31, //   Usage (0x31)
    0x91, 0x02, //   Output (Data,Var,Abs,Non-volatile)
    0x09, 0x3B, //   Usage (0x3B)
    0x81, 0x02, //   Input (Data,Var,Abs)
    0x85, 0x32, //   Report ID (50)
    0x09, 0x32, //   Usage (0x32)
    0x95, 0x8D, //   Report Count (-115)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x33, //   Report ID (51)
    0x09, 0x33, //   Usage (0x33)
    0x95, 0xCD, //   Report Count (-51)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x34, //   Report ID (52)
    0x09, 0x34, //   Usage (0x34)
    0x96, 0x0D, 0x01, //   Report Count (269)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x35, //   Report ID (53)
    0x09, 0x35, //   Usage (0x35)
    0x96, 0x4D, 0x01, //   Report Count (333)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x36, //   Report ID (54)
    0x09, 0x36, //   Usage (0x36)
    0x96, 0x8D, 0x01, //   Report Count (397)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x37, //   Report ID (55)
    0x09, 0x37, //   Usage (0x37)
    0x96, 0xCD, 0x01, //   Report Count (461)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x38, //   Report ID (56)
    0x09, 0x38, //   Usage (0x38)
    0x96, 0x0D, 0x02, //   Report Count (525)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x39, //   Report ID (57)
    0x09, 0x39, //   Usage (0x39)
    0x96, 0x22, 0x02, //   Report Count (546)
    0x91, 0x02, //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x80, 0xFF, //   Usage Page (Vendor Defined 0xFF80)
    0x85, 0x05, //   Report ID (5)
    0x09, 0x33, //   Usage (0x33)
    0x95, 0x29, //   Report Count (41)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x08, //   Report ID (8)
    0x09, 0x34, //   Usage (0x34)
    0x95, 0x2F, //   Report Count (47)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x09, //   Report ID (9)
    0x09, 0x24, //   Usage (0x24)
    0x95, 0x14, //   Report Count (20)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x20, //   Report ID (32)
    0x09, 0x26, //   Usage (0x26)
    0x95, 0x40, //   Report Count (64)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x22, //   Report ID (34)
    0x09, 0x40, //   Usage (0x40)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x80, //   Report ID (-128)
    0x09, 0x28, //   Usage (0x28)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x81, //   Report ID (-127)
    0x09, 0x29, //   Usage (0x29)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x82, //   Report ID (-126)
    0x09, 0x2A, //   Usage (0x2A)
    0x95, 0x09, //   Report Count (9)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x83, //   Report ID (-125)
    0x09, 0x2B, //   Usage (0x2B)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1, //   Report ID (-15)
    0x09, 0x31, //   Usage (0x31)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2, //   Report ID (-14)
    0x09, 0x32, //   Usage (0x32)
    0x95, 0x34, //   Report Count (52)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF0, //   Report ID (-16)
    0x09, 0x30, //   Usage (0x30)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x60, //   Report ID (96)
    0x09, 0x41, //   Usage (0x41)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x61, //   Report ID (97)
    0x09, 0x42, //   Usage (0x42)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x62, //   Report ID (98)
    0x09, 0x43, //   Usage (0x43)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x63, //   Report ID (99)
    0x09, 0x44, //   Usage (0x44)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x64, //   Report ID (100)
    0x09, 0x45, //   Usage (0x45)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x65, //   Report ID (101)
    0x09, 0x46, //   Usage (0x46)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x68, //   Report ID (104)
    0x09, 0x47, //   Usage (0x47)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x70, //   Report ID (112)
    0x09, 0x48, //   Usage (0x48)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x71, //   Report ID (113)
    0x09, 0x49, //   Usage (0x49)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x72, //   Report ID (114)
    0x09, 0x4A, //   Usage (0x4A)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x73, //   Report ID (115)
    0x09, 0x4B, //   Usage (0x4B)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x74, //   Report ID (116)
    0x09, 0x4C, //   Usage (0x4C)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x75, //   Report ID (117)
    0x09, 0x4D, //   Usage (0x4D)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x76, //   Report ID (118)
    0x09, 0x4E, //   Usage (0x4E)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x77, //   Report ID (119)
    0x09, 0x4F, //   Usage (0x4F)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x78, //   Report ID (120)
    0x09, 0x50, //   Usage (0x50)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x79, //   Report ID (121)
    0x09, 0x51, //   Usage (0x51)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x7A, //   Report ID (122)
    0x09, 0x52, //   Usage (0x52)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x7B, //   Report ID (123)
    0x09, 0x53, //   Usage (0x53)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF4, //   Report ID (-12)
    0x09, 0x2C, //   Usage (0x2C)
    0x95, 0x3F, //   Report Count (63)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF5, //   Report ID (-11)
    0x09, 0x2D, //   Usage (0x2D)
    0x95, 0x07, //   Report Count (7)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF6, //   Report ID (-10)
    0x09, 0x2E, //   Usage (0x2E)
    0x96, 0x22, 0x02, //   Report Count (546)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF7, //   Report ID (-9)
    0x09, 0x2F, //   Usage (0x2F)
    0x95, 0x07, //   Report Count (7)
    0xB1, 0x02, //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0 // End Collection
};
static_assert(sizeof(DualsenseEdge_HIDDescriptor) == 428, "Wrong size");
static_assert(sizeof(DualsenseEdge_StockCalibration) == DUALSENSE_CALIBRATION_REPORT_SIZE,
    "DualsenseEdge_StockCalibration must match DUALSENSE_CALIBRATION_REPORT_SIZE and the HID descriptor's Report Count for 0x05");
static_assert(sizeof(DualsenseEdge_FirmwareInfo) == DUALSENSE_FIRMWARE_INFO_REPORT_SIZE,
    "DualsenseEdge_FirmwareInfo must match DUALSENSE_FIRMWARE_INFO_REPORT_SIZE and the HID descriptor's Report Count for 0x20");
static_assert(sizeof(DualsenseEdge_PairInfo_common) == 9,
    "DualsenseEdge_PairInfo_common must be 9 bytes (fills the 9-byte `common` field in DualsenseGamepadPairingReportdata)");
#endif
