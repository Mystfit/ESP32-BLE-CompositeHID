#include "DualsenseGamepadDevice.h"
#include "BleCompositeHID.h"
#include "DualsenseDescriptors.h"
#include "ArduinoDefines.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdint.h>
#include <string.h>
#include "esp_mac.h"
#include "esp_cpu.h"
#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "DualsenseGamepadDevice"
#else
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_log_level.h"

static const char* LOG_TAG = "DualsenseGamepadDevice";
#endif

// The DUALSENSE_BUTTON_* masks live in a 20-bit logical button space. In the wire report the
// low nibble of buttons[0] is occupied by the 4-bit hat, so button bit 0 lands at combined
// bit 4 (= buttons[0] bit 4). These helpers apply the 4-bit shift when reading/writing.
static inline void buttons_set(uint8_t b[3], uint32_t mask)
{
    uint32_t shifted = mask << 4;
    b[0] |= (uint8_t)(shifted & 0xFF);
    b[1] |= (uint8_t)((shifted >> 8) & 0xFF);
    b[2] |= (uint8_t)((shifted >> 16) & 0xFF);
}
static inline void buttons_clear(uint8_t b[3], uint32_t mask)
{
    uint32_t shifted = mask << 4;
    b[0] &= (uint8_t)~(shifted & 0xFF);
    b[1] &= (uint8_t)~((shifted >> 8) & 0xFF);
    b[2] &= (uint8_t)~((shifted >> 16) & 0xFF);
}
static inline bool buttons_test(const uint8_t b[3], uint32_t mask)
{
    uint32_t combined = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
    uint32_t shifted = mask << 4;
    return (combined & shifted) == shifted;
}
static inline void hat_set(uint8_t b[3], uint8_t dir)
{
    b[0] = (uint8_t)((b[0] & 0xF0) | (dir & 0x0F));
}
static inline uint8_t hat_get(const uint8_t b[3])
{
    return (uint8_t)(b[0] & 0x0F);
}

DualsenseGamepadCallbacks::DualsenseGamepadCallbacks(DualsenseGamepadDevice* device)
    : _device(device)
{
}

void DualsenseGamepadCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    // Get the characteristic handle to identify which characteristic was written
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();

    const NimBLEAttValue& value = pCharacteristic->getValue();
    size_t len = value.size();

    if (len == 0) {
        ESP_LOGD(LOG_TAG, "*** onWrite triggered: handle=%d, size=0 (empty) connHandle=%d ***",
            handle, connInfo.getConnHandle());
        return;
    }

    const uint8_t* data = value.data();
    [[maybe_unused]] const char* role = "unknown";
    if (_device) {
        if (pCharacteristic == _device->getOutputChar())         role = "OUTPUT-0x31";
        else if (pCharacteristic == _device->getCalibration())   role = "FEATURE-0x05";
        else if (pCharacteristic == _device->getPairingInfo())   role = "FEATURE-0x09";
        else if (pCharacteristic == _device->getFirmwareInfo())  role = "FEATURE-0x20";
        else if (pCharacteristic == _device->getBtPatchInfo())   role = "FEATURE-0x22";
    }
    // Full hex dump so we can see exactly what the host is writing - critical
    // for confirming whether tools like DSX are sending output reports at all,
    // and for inspecting reserved regions we don't parse into named fields.
    ESP_LOGD(LOG_TAG, "*** onWrite: role=%s handle=%d size=%d connHandle=%d ***",
        role, handle, len, connInfo.getConnHandle());
    ESP_LOG_BUFFER_HEX_LEVEL(LOG_TAG, data, len, ESP_LOG_DEBUG);

    if (len < 47) {
        ESP_LOGW(LOG_TAG, "Output report too small for DualSense: %d bytes (may be feature/LED-only write)", len);
    }

    DualsenseGamepadOutputReportData outputData;

    if (len >= 47 && outputData.load(data, len)) {
        _device->onReceivedOutputReport.fire(outputData);
    }
}

void DualsenseGamepadCallbacks::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    // When Steam/host reads a feature report, we need to populate it with current data
    // The characteristic handle can help identify which report is being read
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();

    ESP_LOGD(LOG_TAG, "*** onRead triggered: handle=%d, connHandle=%d ***", handle, connInfo.getConnHandle());

    // Populate feature reports on read so Steam gets valid data
    // This is called BEFORE the data is sent to the host
    _device->populateFeatureReportOnRead(pCharacteristic);
}

void DualsenseGamepadCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue)
{
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();
    [[maybe_unused]] const char* role = "unknown";

    if (_device) {
        if (pCharacteristic == _device->getInputChar())          role = "INPUT-0x31";
        else if (pCharacteristic == _device->getOutputChar())    role = "OUTPUT-0x31";
        else if (pCharacteristic == _device->getMinimalInput())  role = "INPUT-0x01";
        else if (pCharacteristic == _device->getCalibration())   role = "FEATURE-0x05";
        else if (pCharacteristic == _device->getPairingInfo())   role = "FEATURE-0x09";
        else if (pCharacteristic == _device->getFirmwareInfo())  role = "FEATURE-0x20";
        else if (pCharacteristic == _device->getBtPatchInfo())   role = "FEATURE-0x22";
    }

    ESP_LOGI(LOG_TAG, "*** onSubscribe: role=%s handle=%d subValue=%d (0=off, 1=notify, 2=indicate) ***",
        role, handle, subValue);
}

void DualsenseGamepadCallbacks::onStatus(NimBLECharacteristic* pCharacteristic, int code)
{
    ESP_LOGD(LOG_TAG, "DualsenseGamepadCallbacks::onStatus, code: %d", code);
}

DualsenseGamepadDevice::DualsenseGamepadDevice()
    : _config(new DualsenseEdgeControllerDeviceConfiguration())
    , _extra_input(nullptr)
    , _minimalInput(nullptr)
    , _callbacks(nullptr)
    , _calibration(nullptr)
    , _firmwareInfo(nullptr)
    , _pairingInfo(nullptr)
    , _btPatchInfo(nullptr)

{
}

// DualsenseGamepadDevice methods
DualsenseGamepadDevice::DualsenseGamepadDevice(DualsenseGamepadDeviceConfiguration* config)
    : _config(config)
    , _extra_input(nullptr)
    , _minimalInput(nullptr)
    , _callbacks(nullptr)
    , _calibration(nullptr)
    , _firmwareInfo(nullptr)
    , _pairingInfo(nullptr)
    , _btPatchInfo(nullptr)
{
}

DualsenseGamepadDevice::~DualsenseGamepadDevice()
{
    if (getOutput() && _callbacks) {
        getOutput()->setCallbacks(nullptr);
        delete _callbacks;
        _callbacks = nullptr;
    }

    if (_extra_input) {
        delete _extra_input;
        _extra_input = nullptr;
    }
    if (_firmwareInfo) {
        delete _firmwareInfo;
        _firmwareInfo = nullptr;
    }
    if (_calibration) {
        delete _calibration;
        _calibration = nullptr;
    }
    if (_pairingInfo) {
        delete _pairingInfo;
        _pairingInfo = nullptr;
    }

    if (_config) {
        delete _config;
        _config = nullptr;
    }
}

void DualsenseGamepadDevice::init(NimBLEHIDDevice* hid)
{
    /// Create input characteristic to send events to the computer
    NimBLECharacteristic* input = hid->getInputReport(DUALSENSE_EDGE_INPUT_REPORT_ID);

    // Minimal Generic Desktop input report 0x01. Real DualSense exposes this
    // alongside 0x31 so Windows HIDClass activates the full input pipe.
    _minimalInput = hid->getInputReport(DUALSENSE_MINIMAL_INPUT_REPORT_ID);

    // Create output characteristic to handle events coming from the computer
    NimBLECharacteristic* output = hid->getOutputReport(DUALSENSE_EDGE_OUTPUT_REPORT_ID);
    _callbacks = new DualsenseGamepadCallbacks(this);

    // Set callbacks on all characteristics so we can track reads/writes
    input->setCallbacks(_callbacks);
    _minimalInput->setCallbacks(_callbacks);
    output->setCallbacks(_callbacks);

    // pending callbacks for pairing and stuff
    _calibration = hid->getFeatureReport(DUALSENSE_CALIBRATION_REPORT_ID);
    _calibration->setCallbacks(_callbacks);
    _firmwareInfo = hid->getFeatureReport(DUALSENSE_FIRMWARE_INFO_REPORT_ID);
    _firmwareInfo->setCallbacks(_callbacks);
    _pairingInfo = hid->getFeatureReport(DUALSENSE_PAIRING_INFO_REPORT_ID);
    _pairingInfo->setCallbacks(_callbacks);
    _btPatchInfo = hid->getFeatureReport(DUALSENSE_BT_PATCH_REPORT_ID);
    _btPatchInfo->setCallbacks(_callbacks);
    // memcpy(_pairingReport.mac_address,hid->esp_bt_dev_get_address();
    setCharacteristics(input, output);

    // Log characteristic handles for debugging
    ESP_LOGI(LOG_TAG, "Characteristic handles: input=%d, output=%d, calibration=%d, firmwareInfo=%d, pairingInfo=%d",
        input->getHandle(), output->getHandle(), _calibration->getHandle(),
        _firmwareInfo->getHandle(), _pairingInfo->getHandle());
    m_pCrcTable = new uint32_t[256];
    generate_crc_table(m_pCrcTable);

    // Pre-populate feature reports so they're ready when host reads them
    // This is critical for Steam to recognize controller capabilities (vibration, etc.)
    ESP_LOGI(LOG_TAG, "Pre-populating feature reports...");

    uint8_t calibBuf[DUALSENSE_CALIBRATION_REPORT_SIZE] = {};
    buildFeatureReportWithCrc(DUALSENSE_CALIBRATION_REPORT_ID,
        DualsenseEdge_StockCalibration, DUALSENSE_CALIBRATION_REPORT_SIZE - 5,
        calibBuf, DUALSENSE_CALIBRATION_REPORT_SIZE);
    _calibration->setValue(calibBuf, DUALSENSE_CALIBRATION_REPORT_SIZE);

    uint8_t firmBuf[DUALSENSE_FIRMWARE_INFO_REPORT_SIZE] = {};
    buildFeatureReportWithCrc(DUALSENSE_FIRMWARE_INFO_REPORT_ID,
        DualsenseEdge_FirmwareInfo, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE - 5,
        firmBuf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
    _firmwareInfo->setValue(firmBuf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);

    // Pairing info report (0x09) - includes MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    memcpy(_pairingReport.mac_address, mac, 6);
    memcpy(_pairingReport.common, (uint8_t*)&DualsenseEdge_PairInfo_common, 9);
    uint8_t pairHdr[] = { PS_FEATURE_CRC32_SEED, DUALSENSE_PAIRING_INFO_REPORT_ID };
    uint32_t pairCrc = crc32_le(0xFFFFFFFF, pairHdr, sizeof(pairHdr));
    pairCrc = ~crc32_le(pairCrc, (uint8_t*)&_pairingReport, offsetof(DualsenseGamepadPairingReportdata, crc32));
    _pairingReport.crc32 = pairCrc;
    _pairingInfo->setValue((uint8_t*)&_pairingReport, DUALSENSE_PAIRING_INFO_REPORT_SIZE);

    // BT patch version (0x22) - zero-filled payload + CRC. DSX reads this during
    // handshake; if the characteristic doesn't exist or size mismatches, Windows
    // rejects the GATT read with Win32 error 87 and tears down the HID session.
    uint8_t btPatchBuf[DUALSENSE_BT_PATCH_REPORT_SIZE];
    uint8_t btPatchPayload[DUALSENSE_BT_PATCH_REPORT_SIZE - 4] = { 0 };
    buildFeatureReportWithCrc(DUALSENSE_BT_PATCH_REPORT_ID,
        btPatchPayload, sizeof(btPatchPayload),
        btPatchBuf, DUALSENSE_BT_PATCH_REPORT_SIZE);
    _btPatchInfo->setValue(btPatchBuf, DUALSENSE_BT_PATCH_REPORT_SIZE);

    // Minimal 0x01 input report default: sticks centered (0x80), hat = 8 (centered/null),
    // buttons = 0, triggers = 0. Layout: X,Y,Z,Rz, (hat+pad), btnLo, btnHi, Rx, Ry.
    uint8_t minimalDefault[DUALSENSE_MINIMAL_INPUT_REPORT_SIZE] = {
        0x80, 0x80, 0x80, 0x80,  // X, Y, Z, Rz (sticks centered)
        0x08,                     // hat = 8 (null) in low nibble, high nibble pad
        0x00, 0x00,              // 14 buttons + 6 bits padding
        0x00, 0x00               // Rx, Ry (triggers)
    };
    _minimalInput->setValue(minimalDefault, sizeof(minimalDefault));

    ESP_LOGI(LOG_TAG, "Feature reports pre-populated");
}

void DualsenseGamepadDevice::buildFeatureReportWithCrc(
    uint8_t reportId, const uint8_t* payload, size_t payloadSize,
    uint8_t* outBuffer, size_t outSize)
{
    if (outSize < payloadSize + 4) return;
    memcpy(outBuffer, payload, payloadSize);
    uint8_t hdr[] = { PS_FEATURE_CRC32_SEED, reportId };
    uint32_t crc = crc32_le(0xFFFFFFFF, hdr, sizeof(hdr));
    crc = ~crc32_le(crc, outBuffer, payloadSize);
    outBuffer[payloadSize + 0] = (uint8_t)(crc & 0xFF);
    outBuffer[payloadSize + 1] = (uint8_t)((crc >> 8) & 0xFF);
    outBuffer[payloadSize + 2] = (uint8_t)((crc >> 16) & 0xFF);
    outBuffer[payloadSize + 3] = (uint8_t)((crc >> 24) & 0xFF);
}

const BaseCompositeDeviceConfiguration* DualsenseGamepadDevice::getDeviceConfig() const
{
    // Return the device configuration
    return _config;
}

void DualsenseGamepadDevice::resetInputs()
{
    std::lock_guard<std::mutex> lock(_mutex);
    memset(&_inputReport, 0, sizeof(DualsenseGamepadInputReportData));

    _inputReport.bt = 0x01;  // BLE header: HasHID=1 (contains state data)
    _inputReport.x  = DUALSENSE_AXIS_CENTER_OFFSET;  // left stick X
    _inputReport.y  = DUALSENSE_AXIS_CENTER_OFFSET;  // left stick Y
    _inputReport.rx = DUALSENSE_AXIS_CENTER_OFFSET;  // right stick X
    _inputReport.ry = DUALSENSE_AXIS_CENTER_OFFSET;  // right stick Y
    _inputReport.z  = 0;                              // L2 trigger (rest)
    _inputReport.rz = 0;                              // R2 trigger (rest)
    _inputReport.seq = 0x20;
    _inputReport.buttons[0] = 0;
    _inputReport.buttons[1] = 0;
    _inputReport.buttons[2] = 0;
    hat_set(_inputReport.buttons, DUALSENSE_BUTTON_DPAD_NONE);
    _inputReport.extra_buttons = 0;
    _inputReport.touchpoint_l_contact = 0x80;
    _inputReport.touchpoint_r_contact = 0x80;
    _inputReport.timestamp = 0x7621DD40;
    _inputReport.status = 0x0A;  // 100% battery, discharging

    // Controller at rest: ~1g on the axis facing down.
    _inputReport.accel_x = 0;
    _inputReport.accel_y = -DUALSENSE_ACC_RES_PER_G;
    _inputReport.accel_z = 0;

    _inputReport.gyro_x = 0;
    _inputReport.gyro_y = 0;
    _inputReport.gyro_z = 0;
}

void DualsenseGamepadDevice::press(uint32_t button)
{
    if (!isPressed(button)) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            buttons_set(_inputReport.buttons, button);
            ESP_LOGD(LOG_TAG, "DualsenseGamepadDevice::press, button: %d", button);
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}
void DualsenseGamepadDevice::setLeftTouchpad(uint16_t x, uint16_t y)
{

    if (_inputReport.touchpoint_l_contact != 0x00) {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.touchpoint_l_contact = 0x00;
    }
    _inputReport.touchpoint_l_x = x & 0xFFF;
    _inputReport.touchpoint_l_y = y & 0xFFF;
    if (_config->getAutoReport()) {
        sendGamepadReport();
    }
}
void DualsenseGamepadDevice::setRightTouchpad(uint16_t x, uint16_t y)
{

    if (_inputReport.touchpoint_r_contact != 0x00) {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.touchpoint_r_contact = 0x00;
    }
    _inputReport.touchpoint_r_x = x & 0xFFF;
    _inputReport.touchpoint_r_y = y & 0xFFF;
    if (_config->getAutoReport()) {
        sendGamepadReport();
    }
}
void DualsenseGamepadDevice::releaseLeftTouchpad()
{
    _inputReport.touchpoint_l_contact = 0x80;
    if (_config->getAutoReport()) {
        sendGamepadReport();
    }
}
void DualsenseGamepadDevice::releaseRightTouchpad()
{
    _inputReport.touchpoint_r_contact = 0x80;
    if (_config->getAutoReport()) {
        sendGamepadReport();
    }
}

void DualsenseGamepadDevice::release(uint32_t button)
{
    if (isPressed(button)) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            buttons_clear(_inputReport.buttons, button);
            ESP_LOGD(LOG_TAG, "DualsenseGamepadDevice::release, button: %d", button);
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

bool DualsenseGamepadDevice::isPressed(uint32_t button)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return buttons_test(_inputReport.buttons, button);
}

void DualsenseGamepadDevice::setLeftThumb(int8_t x, int8_t y)
{
    x = constrain(x, DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX);
    y = constrain(y, DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX);

    if (_inputReport.x != x || _inputReport.y != y) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.x = (uint8_t)(x + DUALSENSE_AXIS_CENTER_OFFSET);
            _inputReport.y = (uint8_t)(y + DUALSENSE_AXIS_CENTER_OFFSET);
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setRightThumb(int8_t x, int8_t y)
{
    x = constrain(x, DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX);
    y = constrain(y, DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX);

    uint8_t rx = (uint8_t)(x + DUALSENSE_AXIS_CENTER_OFFSET);
    uint8_t ry = (uint8_t)(y + DUALSENSE_AXIS_CENTER_OFFSET);

    if (_inputReport.rx != rx || _inputReport.ry != ry) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.rx = rx;
            _inputReport.ry = ry;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setLeftTrigger(uint8_t value)
{
    value = constrain(value, DUALSENSE_TRIGGER_MIN, DUALSENSE_TRIGGER_MAX);

    if (_inputReport.z != value) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.z = value;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setRightTrigger(uint8_t value)
{
    value = constrain(value, DUALSENSE_TRIGGER_MIN, DUALSENSE_TRIGGER_MAX);

    if (_inputReport.rz != value) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.rz = value;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setGyro(int16_t pitch, int16_t yaw, int16_t roll)
{
    pitch = constrain(pitch, -DUALSENSE_GYRO_RANGE, DUALSENSE_GYRO_RANGE);
    yaw = constrain(yaw, -DUALSENSE_GYRO_RANGE, DUALSENSE_GYRO_RANGE);
    roll = constrain(roll, -DUALSENSE_GYRO_RANGE, DUALSENSE_GYRO_RANGE);

    if (_inputReport.gyro_x != pitch || _inputReport.gyro_y != yaw || _inputReport.gyro_z != roll) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.gyro_x = pitch;
            _inputReport.gyro_y = yaw;
            _inputReport.gyro_z = roll;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}
void DualsenseGamepadDevice::setAccel(int16_t x, int16_t y, int16_t z)
{
    x = constrain(x, -DUALSENSE_ACC_RANGE, DUALSENSE_ACC_RANGE);
    y = constrain(y, -DUALSENSE_ACC_RANGE, DUALSENSE_ACC_RANGE);
    z = constrain(z, -DUALSENSE_ACC_RANGE, DUALSENSE_ACC_RANGE);

    if (_inputReport.accel_x != x || _inputReport.accel_y != y || _inputReport.accel_z != z) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.accel_x = x;
            _inputReport.accel_y = y;
            _inputReport.accel_z = z;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setBatteryLevel(uint8_t level)
{
    level = constrain(level, DUALSENSE_BATTERY_MIN, DUALSENSE_BATTERY_MAX);
    uint8_t internalLevel = level / 10;  // Convert 0-100 to 0-10

    uint8_t newStatus = (_inputReport.status & 0xF0) | internalLevel;

    if (_inputReport.status != newStatus) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.status = newStatus;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setChargingStatus(bool charging)
{
    uint8_t status = charging ? 1 : 0;  // 1 for charging, 0 for discharging

    uint8_t newStatus = (_inputReport.status & 0x0F) | (status << 4);

    if (_inputReport.status != newStatus) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.status = newStatus;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::setTriggers(uint8_t left, uint8_t right)
{
    left = constrain(left, DUALSENSE_TRIGGER_MIN, DUALSENSE_TRIGGER_MAX);
    right = constrain(right, DUALSENSE_TRIGGER_MIN, DUALSENSE_TRIGGER_MAX);

    if (_inputReport.z != left || _inputReport.rz != right) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.z = left;
            _inputReport.rz = right;
        }
        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::pressDPadDirection(uint8_t direction)
{

    // Avoid double presses
    if (!isDPadPressed(direction)) {
        ESP_LOGD(LOG_TAG, "Pressing dpad direction %s", dPadDirectionName(direction).c_str());
        {
            std::lock_guard<std::mutex> lock(_mutex);
            hat_set(_inputReport.buttons, direction);
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void DualsenseGamepadDevice::pressDPadDirectionFlag(DualsenseDpadFlags direction)
{
    // Filter opposite button presses
    if ((direction & (DualsenseDpadFlags::NORTH | DualsenseDpadFlags::SOUTH)) == (DualsenseDpadFlags::NORTH | DualsenseDpadFlags::SOUTH)) {
        ESP_LOGD(LOG_TAG, "Filtering opposite button presses - up down");
        direction = (DualsenseDpadFlags)(direction ^ (uint8_t)(DualsenseDpadFlags::NORTH | DualsenseDpadFlags::SOUTH));
    }
    if ((direction & (DualsenseDpadFlags::EAST | DualsenseDpadFlags::WEST)) == (DualsenseDpadFlags::EAST | DualsenseDpadFlags::WEST)) {
        ESP_LOGD(LOG_TAG, "Filtering opposite button presses - left right");
        direction = (DualsenseDpadFlags)(direction ^ (uint8_t)(DualsenseDpadFlags::EAST | DualsenseDpadFlags::WEST));
    }

    pressDPadDirection(dPadDirectionToValue(direction));
}

void DualsenseGamepadDevice::releaseDPad()
{
    pressDPadDirection(DUALSENSE_BUTTON_DPAD_NONE);
}

bool DualsenseGamepadDevice::isDPadPressed(uint8_t direction)
{
    std::lock_guard<std::mutex> lock(_mutex);
    return hat_get(_inputReport.buttons) == direction;
}

bool DualsenseGamepadDevice::isDPadPressedFlag(DualsenseDpadFlags direction)
{
    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t hat = hat_get(_inputReport.buttons);

    if (direction == DualsenseDpadFlags::NORTH) {
        return hat == DUALSENSE_BUTTON_DPAD_NORTH;
    } else if (direction == (DualsenseDpadFlags::NORTH | DualsenseDpadFlags::EAST)) {
        return hat == DUALSENSE_BUTTON_DPAD_NORTHEAST;
    } else if (direction == DualsenseDpadFlags::EAST) {
        return hat == DUALSENSE_BUTTON_DPAD_EAST;
    } else if (direction == (DualsenseDpadFlags::SOUTH | DualsenseDpadFlags::EAST)) {
        return hat == DUALSENSE_BUTTON_DPAD_SOUTHEAST;
    } else if (direction == DualsenseDpadFlags::SOUTH) {
        return hat == DUALSENSE_BUTTON_DPAD_SOUTH;
    } else if (direction == (DualsenseDpadFlags::SOUTH | DualsenseDpadFlags::WEST)) {
        return hat == DUALSENSE_BUTTON_DPAD_SOUTHWEST;
    } else if (direction == DualsenseDpadFlags::WEST) {
        return hat == DUALSENSE_BUTTON_DPAD_WEST;
    } else if (direction == (DualsenseDpadFlags::NORTH | DualsenseDpadFlags::WEST)) {
        return hat == DUALSENSE_BUTTON_DPAD_NORTHWEST;
    }
    return false;
}

void DualsenseGamepadDevice::seq()
{
    if (_inputReport.seq < 254) {
        _inputReport.seq += 1;
    } else {
        _inputReport.seq = 0;
    }

    sendGamepadReport();
}
void DualsenseGamepadDevice::timestamp()
{
    uint32_t cycles = esp_cpu_get_cycle_count() / 1500;

    _inputReport.timestamp = cycles;
}

void DualsenseGamepadDevice::sendGamepadReport(bool defer)
{
    if (defer || _config->getAutoDefer()) {
        queueDeferredReport(std::bind(&DualsenseGamepadDevice::sendGamepadReportImpl, this));
    } else {
        sendGamepadReportImpl();
    }
}
void DualsenseGamepadDevice::sendFirmInfoReport(bool defer)
{
    if (defer || _config->getAutoDefer()) {
        queueDeferredReport(std::bind(&DualsenseGamepadDevice::sendFirmInfoReportImpl, this));
    } else {
        sendFirmInfoReportImpl();
    }
}
void DualsenseGamepadDevice::sendCalibrationReport(bool defer)
{
    if (defer || _config->getAutoDefer()) {
        queueDeferredReport(std::bind(&DualsenseGamepadDevice::sendCalibrationReportImpl, this));
    } else {
        sendCalibrationReportImpl();
    }
}
void DualsenseGamepadDevice::sendPairingInfoReport(bool defer)
{
    if (defer || _config->getAutoDefer()) {
        queueDeferredReport(std::bind(&DualsenseGamepadDevice::sendPairingInfoReportImpl, this));
    } else {
        sendPairingInfoReportImpl();
    }
}
void DualsenseGamepadDevice::sendGamepadReportImpl()
{
    auto input = getInput();
    auto parentDevice = this->getParent();

    if (!input || !parentDevice)
        return;

    if (!parentDevice->isConnected())
        return;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        [[maybe_unused]] size_t packedSize = sizeof(_inputReport);
        ESP_LOGD(LOG_TAG, "Sending gamepad report, size: %d", packedSize);
        uint8_t bthdr[] = { PS_INPUT_CRC32_SEED, DUALSENSE_EDGE_INPUT_REPORT_ID };
        uint32_t crc = 0;
        crc = this->crc32_le(0xFFFFFFFF, (uint8_t*)&bthdr, sizeof(bthdr));
        crc = ~this->crc32_le(crc, (uint8_t*)&_inputReport, sizeof(_inputReport) - 4);
        _inputReport.crc32 = crc;
        ESP_LOGD(LOG_TAG, "got input CRC %lx", crc);
        input->setValue((uint8_t*)&_inputReport, sizeof(_inputReport));
    }
    input->notify();
}
void DualsenseGamepadDevice::sendFirmInfoReportImpl()
{
    auto firminfo = _firmwareInfo;
    auto parentDevice = this->getParent();

    if (!firminfo || !parentDevice)
        return;

    if (!parentDevice->isConnected())
        return;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t packedSize = DUALSENSE_FIRMWARE_INFO_REPORT_SIZE;
        ESP_LOGD(LOG_TAG, "Sending firmware info report, size: %d", packedSize);
        uint8_t buf[DUALSENSE_FIRMWARE_INFO_REPORT_SIZE] = {};
        buildFeatureReportWithCrc(DUALSENSE_FIRMWARE_INFO_REPORT_ID,
            DualsenseEdge_FirmwareInfo, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE - 5,
            buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
        firminfo->setValue(buf, packedSize);
    }
    firminfo->indicate();
}
void DualsenseGamepadDevice::sendCalibrationReportImpl()
{
    auto calibration = _calibration;
    auto parentDevice = this->getParent();

    if (!calibration || !parentDevice)
        return;

    if (!parentDevice->isConnected())
        return;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t packedSize = DUALSENSE_CALIBRATION_REPORT_SIZE;
        ESP_LOGD(LOG_TAG, "Sending calibration report, size: %d", packedSize);
        uint8_t buf[DUALSENSE_CALIBRATION_REPORT_SIZE] = {};
        buildFeatureReportWithCrc(DUALSENSE_CALIBRATION_REPORT_ID,
            DualsenseEdge_StockCalibration, DUALSENSE_CALIBRATION_REPORT_SIZE - 5,
            buf, DUALSENSE_CALIBRATION_REPORT_SIZE);
        calibration->setValue(buf, packedSize);
    }
    calibration->indicate();
}
void DualsenseGamepadDevice::sendPairingInfoReportImpl()
{
    ESP_LOGD(LOG_TAG, "Sending Pairing report");
    auto pairinginfo = _pairingInfo;
    auto parentDevice = this->getParent();
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    memcpy(_pairingReport.mac_address, mac, 6);
    memcpy(_pairingReport.common, (uint8_t*)&DualsenseEdge_PairInfo_common, 9);
    if (!pairinginfo || !parentDevice)
        return;

    if (!parentDevice->isConnected())
        return;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        uint8_t bthdr[] = { PS_FEATURE_CRC32_SEED, DUALSENSE_PAIRING_INFO_REPORT_ID };
        uint32_t crc = 0;
        crc = this->crc32_le(0xFFFFFFFF, (uint8_t*)&bthdr, sizeof(bthdr));
        crc = ~this->crc32_le(crc, (uint8_t*)&_pairingReport, offsetof(DualsenseGamepadPairingReportdata, crc32));
        _pairingReport.crc32 = crc;
        ESP_LOGD(LOG_TAG, "got pairing CRC %lx", crc);
        size_t packedSize = DUALSENSE_PAIRING_INFO_REPORT_SIZE;
        ESP_LOGD(LOG_TAG, "Sending Pair report, size: %d", packedSize);
        pairinginfo->setValue((uint8_t*)&_pairingReport, packedSize);
    }
    pairinginfo->indicate();
}

void DualsenseGamepadDevice::populateFeatureReportOnRead(NimBLECharacteristic* pCharacteristic)
{
    // Identify which feature report is being read and populate it
    // This ensures Steam/host gets valid data when it reads feature reports

    // Debug: log the handles to help identify which characteristic is which
    ESP_LOGD(LOG_TAG, "populateFeatureReportOnRead: pChar=%p, _output=%p, _calibration=%p, _firmwareInfo=%p, _pairingInfo=%p, _btPatchInfo=%p",
        (void*)pCharacteristic, (void*)getOutput(), (void*)_calibration, (void*)_firmwareInfo, (void*)_pairingInfo, (void*)_btPatchInfo);

    if (pCharacteristic == getOutput()) {
        // Steam reads from output characteristic to check if device supports output reports
        // Provide an empty/default output report structure
        ESP_LOGI(LOG_TAG, "Host reading output report (0x31) - providing default data");
        // Return a valid but empty output report (78 bytes for BLE format)
        static uint8_t defaultOutputReport[DS_OUTPUT_REPORT_BT_SIZE] = {0};
        defaultOutputReport[0] = 0x31;  // Report ID
        defaultOutputReport[2] = 0x10;  // Tag (DS_OUTPUT_TAG)
        std::lock_guard<std::mutex> lock(_mutex);
        pCharacteristic->setValue(defaultOutputReport, DS_OUTPUT_REPORT_BT_SIZE);
    }
    else if (pCharacteristic == _calibration) {
        ESP_LOGD(LOG_TAG, "Host reading calibration report (0x05) - populating data and sending input report");
        {
            std::lock_guard<std::mutex> lock(_mutex);
            uint8_t buf[DUALSENSE_CALIBRATION_REPORT_SIZE] = {};
            buildFeatureReportWithCrc(DUALSENSE_CALIBRATION_REPORT_ID,
                DualsenseEdge_StockCalibration, DUALSENSE_CALIBRATION_REPORT_SIZE - 5,
                buf, DUALSENSE_CALIBRATION_REPORT_SIZE);
            pCharacteristic->setValue(buf, DUALSENSE_CALIBRATION_REPORT_SIZE);
        }
        // When calibration is read, also send an input report with valid sensor data
        // This helps Steam associate the calibration with valid gyro/accel readings
        sendGamepadReportImpl();
    }
    else if (pCharacteristic == _firmwareInfo) {
        ESP_LOGI(LOG_TAG, "Host reading firmware info report (0x20) - populating data");
        std::lock_guard<std::mutex> lock(_mutex);
        uint8_t buf[DUALSENSE_FIRMWARE_INFO_REPORT_SIZE] = {};
        buildFeatureReportWithCrc(DUALSENSE_FIRMWARE_INFO_REPORT_ID,
            DualsenseEdge_FirmwareInfo, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE - 5,
            buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
        pCharacteristic->setValue(buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
    }
    else if (pCharacteristic == _pairingInfo) {
        ESP_LOGI(LOG_TAG, "Host reading pairing info report (0x09) - populating data");
        // Build pairing report with MAC address
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_BT);
        memcpy(_pairingReport.mac_address, mac, 6);
        memcpy(_pairingReport.common, (uint8_t*)&DualsenseEdge_PairInfo_common, 9);

        // Calculate CRC
        uint8_t bthdr[] = { PS_FEATURE_CRC32_SEED, DUALSENSE_PAIRING_INFO_REPORT_ID };
        uint32_t crc = 0;
        crc = this->crc32_le(0xFFFFFFFF, (uint8_t*)&bthdr, sizeof(bthdr));
        crc = ~this->crc32_le(crc, (uint8_t*)&_pairingReport, offsetof(DualsenseGamepadPairingReportdata, crc32));
        _pairingReport.crc32 = crc;

        std::lock_guard<std::mutex> lock(_mutex);
        pCharacteristic->setValue((uint8_t*)&_pairingReport, DUALSENSE_PAIRING_INFO_REPORT_SIZE);
    }
    else if (pCharacteristic == _btPatchInfo) {
        ESP_LOGI(LOG_TAG, "Host reading BT patch version report (0x22) - populating data");
        std::lock_guard<std::mutex> lock(_mutex);
        uint8_t buf[DUALSENSE_BT_PATCH_REPORT_SIZE];
        uint8_t payload[DUALSENSE_BT_PATCH_REPORT_SIZE - 4] = { 0 };
        buildFeatureReportWithCrc(DUALSENSE_BT_PATCH_REPORT_ID,
            payload, sizeof(payload),
            buf, DUALSENSE_BT_PATCH_REPORT_SIZE);
        pCharacteristic->setValue(buf, DUALSENSE_BT_PATCH_REPORT_SIZE);
    }
    else if (pCharacteristic == getInput()) {
        // Input report being read - this is normal, Steam may read it to check current state
        ESP_LOGI(LOG_TAG, "Host reading input report - providing current state");
        // The input report is already populated with current gamepad state
        // No additional action needed
    }
    else {
        ESP_LOGW(LOG_TAG, "Unknown feature report read request (handle=%d)", pCharacteristic->getHandle());
    }
}

// taken from https://github.com/StryderUK/BluetoothHID/blob/main/examples/DualShock4
uint32_t DualsenseGamepadDevice::crc32_le(unsigned int crc, unsigned char const* buf, unsigned int len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        crc = m_pCrcTable[(crc ^ buf[i]) & 0xff] ^ (crc >> 8);
    }
    return crc;
}
void DualsenseGamepadDevice::generate_crc_table(uint32_t* crcTable)
{
    const uint32_t POLYNOMIAL = 0xEDB88320; // 0x04C11DB7 reversed
    uint32_t remainder;
    uint8_t b = 0;
    do {
        // Start with the data byte
        remainder = b;
        for (unsigned long bit = 8; bit > 0; --bit) {
            if (remainder & 1)
                remainder = (remainder >> 1) ^ POLYNOMIAL;
            else
                remainder = (remainder >> 1);
        }
        crcTable[(size_t)b] = remainder;
    } while (0 != ++b);
}
