#include "DualsenseGamepadDevice.h"
#include "BleCompositeHID.h"
#include "DualsenseDescriptors.h"
#include "ArduinoDefines.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string.h>
#include "esp_mac.h"
#include "esp_cpu.h"
#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "DualsenseEdgeGamepadDevice"
#else
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_log.h"
#include "esp_log_level.h"
static const char* LOG_TAG = "DualsenseEdgeGamepadDevice";
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
static inline void hat_set(uint8_t b[3], uint8_t dir)
{
    b[0] = (uint8_t)((b[0] & 0xF0) | (dir & 0x0F));
}

// ---- DualsenseEdgeGamepadCallbacks ----

DualsenseEdgeGamepadCallbacks::DualsenseEdgeGamepadCallbacks(DualsenseEdgeGamepadDevice* device)
    : _device(device)
{}

void DualsenseEdgeGamepadCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();
    const NimBLEAttValue& value = pCharacteristic->getValue();
    size_t len = value.size();

    if (len == 0) {
        ESP_LOGD(LOG_TAG, "*** onWrite handle=%d size=0 connHandle=%d ***",
            handle, connInfo.getConnHandle());
        return;
    }

    const uint8_t* data = value.data();
    [[maybe_unused]] const char* role = "unknown";
    if (_device) {
        if (pCharacteristic == _device->getOutputChar())        role = "OUTPUT-0x31";
        else if (pCharacteristic == _device->getCalibration())  role = "FEATURE-0x05";
        else if (pCharacteristic == _device->getPairingInfo())  role = "FEATURE-0x09";
        else if (pCharacteristic == _device->getFirmwareInfo()) role = "FEATURE-0x20";
        else if (pCharacteristic == _device->getBtPatchInfo())  role = "FEATURE-0x22";
    }
    ESP_LOGD(LOG_TAG, "*** onWrite: role=%s handle=%d size=%d connHandle=%d ***",
        role, handle, len, connInfo.getConnHandle());
    // Full hex dump so we can see exactly what the host is writing - critical
    // for confirming whether tools like DSX are sending output reports at all,
    // and for inspecting reserved regions we don't parse into named fields.
    ESP_LOG_BUFFER_HEX_LEVEL(LOG_TAG, data, len, ESP_LOG_DEBUG);

    // Auth challenge (0xF0): PS5 sends the challenge for the DualSense to sign.
    if (_device && pCharacteristic == _device->getAuthPayloadChar()) {
        ESP_LOGI(LOG_TAG, "Auth challenge received (%d bytes)", (int)len);
        DsAuthPayload ap{};
        ap.len = len < sizeof(ap.data) ? len : sizeof(ap.data);
        memcpy(ap.data, data, ap.len);
        _device->onAuthChallenge.fire(ap);
        return;
    }

    if (len < 47) {
        ESP_LOGW(LOG_TAG, "Output report too small: %d bytes", len);
    }

    DualsenseGamepadOutputReportData outputData;
    if (len >= 47 && outputData.load(data, len)) {
        // Dispatch to component Signals
        if (outputData.hasRumble()) {
            _device->rumble().onRumble.fire(
                {outputData.weakMotor(), outputData.strongMotor()});
        }
        if (outputData.hasLightbar()) {
            _device->light().onColorChanged.fire(
                {outputData.lightbar_red, outputData.lightbar_green, outputData.lightbar_blue});
        }
        if (outputData.hasPlayerIndicator()) {
            _device->playerIndicator().onChanged.fire(outputData.player_leds);
        }
        if (outputData.hasLeftTriggerEffect()) {
            _device->leftTrigger().onEffect.fire(outputData.leftTrigger());
        }
        if (outputData.hasRightTriggerEffect()) {
            _device->rightTrigger().onEffect.fire(outputData.rightTrigger());
        }
        if (outputData.valid_flag0 & DS_OUT_FLAG0_SPEAKER_VOLUME) {
            _device->speaker().onVolumeChanged.fire(outputData.speaker_volume);
        }
        if (outputData.valid_flag0 & DS_OUT_FLAG0_MIC_VOLUME) {
            _device->microphone().onVolumeChanged.fire(outputData.mic_volume);
        }
        if (outputData.hasMicMuteLed()) {
            _device->microphone().onMuteChanged.fire(
                (outputData.mute_button_led & 0x01) != 0);
        }
    }
}

void DualsenseEdgeGamepadCallbacks::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();
    ESP_LOGD(LOG_TAG, "*** onRead handle=%d connHandle=%d ***", handle, connInfo.getConnHandle());
    // Populate feature reports on read so Steam gets valid data
    // This is called BEFORE the data is sent to the host
    _device->populateFeatureReportOnRead(pCharacteristic);
}

void DualsenseEdgeGamepadCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue)
{
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();
    ESP_LOGD(LOG_TAG, "onSubscribe handle=%d sub=%d connHandle=%d", handle, subValue, connInfo.getConnHandle());
}

void DualsenseEdgeGamepadCallbacks::onStatus(NimBLECharacteristic* pCharacteristic, int code)
{
    ESP_LOGD(LOG_TAG, "onStatus handle=%d code=%d", pCharacteristic->getHandle(), code);
}

// ---- DualsenseEdgeGamepadDevice ----

DualsenseEdgeGamepadDevice::DualsenseEdgeGamepadDevice() :
    _inputReport{},
    _cross         (_inputReport.buttons, DUALSENSE_BUTTON_A),
    _circle        (_inputReport.buttons, DUALSENSE_BUTTON_B),
    _square        (_inputReport.buttons, DUALSENSE_BUTTON_X),
    _triangle      (_inputReport.buttons, DUALSENSE_BUTTON_Y),
    _l1            (_inputReport.buttons, DUALSENSE_BUTTON_LB),
    _r1            (_inputReport.buttons, DUALSENSE_BUTTON_RB),
    _select        (_inputReport.buttons, DUALSENSE_BUTTON_SELECT),
    _start         (_inputReport.buttons, DUALSENSE_BUTTON_START),
    _home          (_inputReport.buttons, DUALSENSE_BUTTON_MODE),
    _l3            (_inputReport.buttons, DUALSENSE_BUTTON_LS),
    _r3            (_inputReport.buttons, DUALSENSE_BUTTON_RS),
    _touchpadBtn   (_inputReport.buttons, DUALSENSE_BUTTON_TOUCHPAD),
    _share         (_inputReport.buttons, DUALSENSE_BUTTON_SHARE),
    _mute          (_inputReport.buttons, DUALSENSE_BUTTON_MUTE),
    _l4            (_inputReport.buttons, DUALSENSE_BUTTON_L4),
    _r4            (_inputReport.buttons, DUALSENSE_BUTTON_R4),
    _l5            (_inputReport.buttons, DUALSENSE_BUTTON_L5),
    _r5            (_inputReport.buttons, DUALSENSE_BUTTON_R5),
    _leftTrigger   (_inputReport.z),
    _rightTrigger  (_inputReport.rz),
    _leftStick  (_inputReport.x,  _inputReport.y,  DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX),
    _rightStick (_inputReport.rx, _inputReport.ry, DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX),
    _dpad          (_inputReport.buttons),
    _battery       (_inputReport.status),
    _config      (new DualsenseEdgeControllerDeviceConfiguration()),
    _extra_input (nullptr),
    _minimalInput(nullptr),
    _callbacks   (nullptr),
    _calibration (nullptr),
    _firmwareInfo(nullptr),
    _pairingInfo (nullptr),
    _btPatchInfo (nullptr),
    _authF0      (nullptr),
    _authF1      (nullptr),
    _authF2      (nullptr),
    m_pCrcTable  (nullptr)
{
    _inputReport.bt  = 0x01;  // BLE header: HasHID=1 (contains state data)
    _inputReport.x   = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.y   = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.rx  = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.ry  = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.seq = 0x20;
    hat_set(_inputReport.buttons, DUALSENSE_BUTTON_DPAD_NONE);
    _inputReport.touchpoint_0_contact = 0x80;
    _inputReport.touchpoint_1_contact = 0x80;
    _inputReport.timestamp = 0x7621DD40;
    _inputReport.status    = 0x0A;  // 100% battery, discharging
    // Controller at rest: ~1g on the axis facing down.
    _inputReport.accel_y   = -DUALSENSE_ACC_RES_PER_G;

}

DualsenseEdgeGamepadDevice::DualsenseEdgeGamepadDevice(DualsenseEdgeControllerDeviceConfiguration* config) :
    _inputReport{},
    _cross         (_inputReport.buttons, DUALSENSE_BUTTON_A),
    _circle        (_inputReport.buttons, DUALSENSE_BUTTON_B),
    _square        (_inputReport.buttons, DUALSENSE_BUTTON_X),
    _triangle      (_inputReport.buttons, DUALSENSE_BUTTON_Y),
    _l1            (_inputReport.buttons, DUALSENSE_BUTTON_LB),
    _r1            (_inputReport.buttons, DUALSENSE_BUTTON_RB),
    _select        (_inputReport.buttons, DUALSENSE_BUTTON_SELECT),
    _start         (_inputReport.buttons, DUALSENSE_BUTTON_START),
    _home          (_inputReport.buttons, DUALSENSE_BUTTON_MODE),
    _l3            (_inputReport.buttons, DUALSENSE_BUTTON_LS),
    _r3            (_inputReport.buttons, DUALSENSE_BUTTON_RS),
    _touchpadBtn   (_inputReport.buttons, DUALSENSE_BUTTON_TOUCHPAD),
    _share         (_inputReport.buttons, DUALSENSE_BUTTON_SHARE),
    _mute          (_inputReport.buttons, DUALSENSE_BUTTON_MUTE),
    _l4            (_inputReport.buttons, DUALSENSE_BUTTON_L4),
    _r4            (_inputReport.buttons, DUALSENSE_BUTTON_R4),
    _l5            (_inputReport.buttons, DUALSENSE_BUTTON_L5),
    _r5            (_inputReport.buttons, DUALSENSE_BUTTON_R5),
    _leftTrigger   (_inputReport.z),
    _rightTrigger  (_inputReport.rz),
    _leftStick  (_inputReport.x,  _inputReport.y,  DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX),
    _rightStick (_inputReport.rx, _inputReport.ry, DUALSENSE_STICK_MIN, DUALSENSE_STICK_MAX),
    _dpad          (_inputReport.buttons),
    _battery       (_inputReport.status),
    _config      (config),
    _extra_input (nullptr),
    _minimalInput(nullptr),
    _callbacks   (nullptr),
    _calibration (nullptr),
    _firmwareInfo(nullptr),
    _pairingInfo (nullptr),
    _btPatchInfo (nullptr),
    _authF0      (nullptr),
    _authF1      (nullptr),
    _authF2      (nullptr),
    m_pCrcTable  (nullptr)
{
    _inputReport.bt  = 0x01;  // BLE header: HasHID=1 (contains state data)
    _inputReport.x   = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.y   = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.rx  = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.ry  = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.seq = 0x20;
    hat_set(_inputReport.buttons, DUALSENSE_BUTTON_DPAD_NONE);
    _inputReport.touchpoint_0_contact = 0x80;
    _inputReport.touchpoint_1_contact = 0x80;
    _inputReport.timestamp = 0x7621DD40;
    _inputReport.status    = 0x0A;  // 100% battery, discharging
    // Controller at rest: ~1g on the axis facing down.
    _inputReport.accel_y   = -DUALSENSE_ACC_RES_PER_G;

}

DualsenseEdgeGamepadDevice::~DualsenseEdgeGamepadDevice()
{
    if (getOutput() && _callbacks) {
        getOutput()->setCallbacks(nullptr);
        delete _callbacks;
        _callbacks = nullptr;
    }
    if (_config)      { delete _config;      _config = nullptr; }
    if (m_pCrcTable)  { delete[] m_pCrcTable; m_pCrcTable = nullptr; }
}

void DualsenseEdgeGamepadDevice::init(NimBLEHIDDevice* hid)
{
    NimBLECharacteristic* input = hid->getInputReport(DUALSENSE_EDGE_INPUT_REPORT_ID);
    // Minimal Generic Desktop input report 0x01. Real DualSense exposes this
    // alongside 0x31 so Windows HIDClass activates the full input pipe.
    _minimalInput = hid->getInputReport(DUALSENSE_MINIMAL_INPUT_REPORT_ID);
    NimBLECharacteristic* output = hid->getOutputReport(DUALSENSE_EDGE_OUTPUT_REPORT_ID);
    _callbacks = new DualsenseEdgeGamepadCallbacks(this);

    input->setCallbacks(_callbacks);
    _minimalInput->setCallbacks(_callbacks);
    output->setCallbacks(_callbacks);

    _calibration  = hid->getFeatureReport(DUALSENSE_CALIBRATION_REPORT_ID);
    _calibration->setCallbacks(_callbacks);
    _firmwareInfo = hid->getFeatureReport(DUALSENSE_FIRMWARE_INFO_REPORT_ID);
    _firmwareInfo->setCallbacks(_callbacks);
    _pairingInfo  = hid->getFeatureReport(DUALSENSE_PAIRING_INFO_REPORT_ID);
    _pairingInfo->setCallbacks(_callbacks);
    _btPatchInfo  = hid->getFeatureReport(DUALSENSE_BT_PATCH_REPORT_ID);
    _btPatchInfo->setCallbacks(_callbacks);

    // Auth forwarding: 0xF0 (challenge from PS5), 0xF1 (nonce to PS5), 0xF2 (state to PS5).
    // Zero-initialise F1/F2 so PS5 gets a stable "not ready" response until the DualSense signs.
    _authF0 = hid->getFeatureReport(0xF0);
    _authF0->setCallbacks(_callbacks);
    _authF1 = hid->getFeatureReport(0xF1);
    _authF1->setCallbacks(_callbacks);
    {
        uint8_t zeros[63] = {};
        _authF1->setValue(zeros, sizeof(zeros));
    }
    _authF2 = hid->getFeatureReport(0xF2);
    _authF2->setCallbacks(_callbacks);
    {
        uint8_t zeros[52] = {};
        _authF2->setValue(zeros, sizeof(zeros));
    }

    setCharacteristics(input, output);

    ESP_LOGI(LOG_TAG, "Characteristic handles: input=%d output=%d calibration=%d firmwareInfo=%d pairingInfo=%d",
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

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    memcpy(_pairingReport.mac_address, mac, 6);
    memcpy(_pairingReport.common, (uint8_t*)&DualsenseEdge_PairInfo_common, 9);
    uint8_t pairHdr[] = { PS_FEATURE_CRC32_SEED, DUALSENSE_PAIRING_INFO_REPORT_ID };
    uint32_t pairCrc  = crc32_le(0xFFFFFFFF, pairHdr, sizeof(pairHdr));
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

const BaseCompositeDeviceConfiguration* DualsenseEdgeGamepadDevice::getDeviceConfig() const
{
    return _config;
}

void DualsenseEdgeGamepadDevice::resetInputs()
{
    std::lock_guard<std::mutex> lock(_mutex);
    memset(&_inputReport, 0, sizeof(DualsenseGamepadInputReportData));
    _inputReport.bt    = 0x01;
    _inputReport.x     = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.y     = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.rx    = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.ry    = DUALSENSE_AXIS_CENTER_OFFSET;
    _inputReport.seq   = 0x20;
    hat_set(_inputReport.buttons, DUALSENSE_BUTTON_DPAD_NONE);
    _inputReport.touchpoint_0_contact = 0x80;
    _inputReport.touchpoint_1_contact = 0x80;
    _inputReport.timestamp = 0x7621DD40;
    _inputReport.status    = 0x0A;
    _inputReport.accel_y   = -DUALSENSE_ACC_RES_PER_G;
    _touchPointActive[0] = false;
    _touchPointActive[1] = false;
}

int8_t DualsenseEdgeGamepadDevice::touchpadStartTouch(uint16_t x, uint16_t y)
{
    int8_t slotIndex = -1;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (int i = 0; i < 2; i++) {
            if (!_touchPointActive[i]) { slotIndex = (int8_t)i; break; }
        }
        if (slotIndex < 0) return -1;

        _touchPointActive[slotIndex] = true;
        _touchPointId[slotIndex] = _nextTouchId;
        _nextTouchId = (_nextTouchId + 1) & 0x7F;

        // Contact byte: bit 7 = 0 (active), bits 0-6 = touch ID
        uint8_t contact = _touchPointId[slotIndex] & 0x7F;
        if (slotIndex == 0) {
            _inputReport.touchpoint_0_contact = contact;
            _inputReport.touchpoint_0_x = x & 0xFFF;
            _inputReport.touchpoint_0_y = y & 0xFFF;
        } else {
            _inputReport.touchpoint_1_contact = contact;
            _inputReport.touchpoint_1_x = x & 0xFFF;
            _inputReport.touchpoint_1_y = y & 0xFFF;
        }
    }
    if (_config->getAutoReport()) sendReport();
    return slotIndex;
}

void DualsenseEdgeGamepadDevice::touchpadUpdatePosition(uint16_t x, uint16_t y, uint8_t slotIndex)
{
    if (slotIndex >= 2) return;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_touchPointActive[slotIndex]) return;
        if (slotIndex == 0) {
            _inputReport.touchpoint_0_x = x & 0xFFF;
            _inputReport.touchpoint_0_y = y & 0xFFF;
        } else {
            _inputReport.touchpoint_1_x = x & 0xFFF;
            _inputReport.touchpoint_1_y = y & 0xFFF;
        }
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::touchpadStopTouch(uint8_t slotIndex)
{
    if (slotIndex >= 2) return;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_touchPointActive[slotIndex]) return;
        _touchPointActive[slotIndex] = false;
        if (slotIndex == 0) _inputReport.touchpoint_0_contact = 0x80;
        else                 _inputReport.touchpoint_1_contact = 0x80;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setAccel(int16_t x, int16_t y, int16_t z)
{
    x = constrain(x, -DUALSENSE_ACC_RANGE, DUALSENSE_ACC_RANGE);
    y = constrain(y, -DUALSENSE_ACC_RANGE, DUALSENSE_ACC_RANGE);
    z = constrain(z, -DUALSENSE_ACC_RANGE, DUALSENSE_ACC_RANGE);
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.accel_x = x;
        _inputReport.accel_y = y;
        _inputReport.accel_z = z;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setGyro(int16_t pitch, int16_t yaw, int16_t roll)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.gyro_x = pitch;
        _inputReport.gyro_y = yaw;
        _inputReport.gyro_z = roll;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::timestamp()
{
    _inputReport.timestamp = esp_cpu_get_cycle_count() / 1500;
}

void DualsenseEdgeGamepadDevice::seq()
{
    if (_inputReport.seq < 254) _inputReport.seq++;
    else                         _inputReport.seq = 0;
    sendReport();
}

void DualsenseEdgeGamepadDevice::sendReport(bool defer)
{
    if (defer || _config->getAutoDefer()) {
        queueDeferredReport(std::bind(&DualsenseEdgeGamepadDevice::sendGamepadReportImpl, this));
    } else {
        sendGamepadReportImpl();
    }
}

void DualsenseEdgeGamepadDevice::sendFirmInfoReport(bool defer)
{
    if (defer || _config->getAutoDefer())
        queueDeferredReport(std::bind(&DualsenseEdgeGamepadDevice::sendFirmInfoReportImpl, this));
    else
        sendFirmInfoReportImpl();
}

void DualsenseEdgeGamepadDevice::sendCalibrationReport(bool defer)
{
    if (defer || _config->getAutoDefer())
        queueDeferredReport(std::bind(&DualsenseEdgeGamepadDevice::sendCalibrationReportImpl, this));
    else
        sendCalibrationReportImpl();
}

void DualsenseEdgeGamepadDevice::sendPairingInfoReport(bool defer)
{
    if (defer || _config->getAutoDefer())
        queueDeferredReport(std::bind(&DualsenseEdgeGamepadDevice::sendPairingInfoReportImpl, this));
    else
        sendPairingInfoReportImpl();
}

void DualsenseEdgeGamepadDevice::sendGamepadReportImpl()
{
    auto input = getInput();
    auto parentDevice = getParent();
    if (!input || !parentDevice || !parentDevice->isConnected())
        return;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        uint8_t bthdr[] = { PS_INPUT_CRC32_SEED, DUALSENSE_EDGE_INPUT_REPORT_ID };
        uint32_t crc = crc32_le(0xFFFFFFFF, bthdr, sizeof(bthdr));
        crc = ~crc32_le(crc, (uint8_t*)&_inputReport, sizeof(_inputReport) - 4);
        _inputReport.crc32 = crc;
        ESP_LOGD(LOG_TAG, "Sending report size=%d crc=%lx", sizeof(_inputReport), crc);
        input->setValue((uint8_t*)&_inputReport, sizeof(_inputReport));
    }
    input->notify();
}

void DualsenseEdgeGamepadDevice::sendFirmInfoReportImpl()
{
    auto parentDevice = getParent();
    if (!_firmwareInfo || !parentDevice || !parentDevice->isConnected()) return;
    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t buf[DUALSENSE_FIRMWARE_INFO_REPORT_SIZE] = {};
    buildFeatureReportWithCrc(DUALSENSE_FIRMWARE_INFO_REPORT_ID,
        DualsenseEdge_FirmwareInfo, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE - 5,
        buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
    _firmwareInfo->setValue(buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
    _firmwareInfo->indicate();
}

void DualsenseEdgeGamepadDevice::sendCalibrationReportImpl()
{
    auto parentDevice = getParent();
    if (!_calibration || !parentDevice || !parentDevice->isConnected()) return;
    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t buf[DUALSENSE_CALIBRATION_REPORT_SIZE] = {};
    buildFeatureReportWithCrc(DUALSENSE_CALIBRATION_REPORT_ID,
        DualsenseEdge_StockCalibration, DUALSENSE_CALIBRATION_REPORT_SIZE - 5,
        buf, DUALSENSE_CALIBRATION_REPORT_SIZE);
    _calibration->setValue(buf, DUALSENSE_CALIBRATION_REPORT_SIZE);
    _calibration->indicate();
}

void DualsenseEdgeGamepadDevice::sendPairingInfoReportImpl()
{
    auto parentDevice = getParent();
    if (!_pairingInfo || !parentDevice || !parentDevice->isConnected()) return;
    std::lock_guard<std::mutex> lock(_mutex);
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_BT);
    memcpy(_pairingReport.mac_address, mac, 6);
    memcpy(_pairingReport.common, (uint8_t*)&DualsenseEdge_PairInfo_common, 9);
    uint8_t bthdr[] = { PS_FEATURE_CRC32_SEED, DUALSENSE_PAIRING_INFO_REPORT_ID };
    uint32_t crc = crc32_le(0xFFFFFFFF, bthdr, sizeof(bthdr));
    crc = ~crc32_le(crc, (uint8_t*)&_pairingReport, offsetof(DualsenseGamepadPairingReportdata, crc32));
    _pairingReport.crc32 = crc;
    _pairingInfo->setValue((uint8_t*)&_pairingReport, DUALSENSE_PAIRING_INFO_REPORT_SIZE);
    _pairingInfo->indicate();
}

static uint8_t setStatusBit(uint8_t current, uint8_t mask, bool value)
{
    return value ? (current | mask) : (current & ~mask);
}

void DualsenseEdgeGamepadDevice::setL2TriggerFeedback(uint8_t effectFlags, uint8_t position, uint8_t reserved)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.l2_trigger_feedback[0] = effectFlags;
        _inputReport.l2_trigger_feedback[1] = position;
        _inputReport.l2_trigger_feedback[2] = reserved;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setR2TriggerFeedback(uint8_t effectFlags, uint8_t position, uint8_t reserved)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.r2_trigger_feedback[0] = effectFlags;
        _inputReport.r2_trigger_feedback[1] = position;
        _inputReport.r2_trigger_feedback[2] = reserved;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setHeadphonesPlugged(bool plugged)
{
    uint8_t newStatus2 = setStatusBit(_inputReport.status2, DUALSENSE_STATUS2_HEADPHONES_PLUGGED, plugged);
    if (_inputReport.status2 != newStatus2) {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.status2 = newStatus2;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setHeadphoneMic(bool connected)
{
    uint8_t newStatus2 = setStatusBit(_inputReport.status2, DUALSENSE_STATUS2_HEADPHONE_MIC, connected);
    if (_inputReport.status2 != newStatus2) {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.status2 = newStatus2;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setMuteActive(bool active)
{
    uint8_t newStatus2 = setStatusBit(_inputReport.status2, DUALSENSE_STATUS2_MUTE_ACTIVE, active);
    if (_inputReport.status2 != newStatus2) {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.status2 = newStatus2;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setUsbPlugged(bool plugged)
{
    uint8_t newStatus2 = setStatusBit(_inputReport.status2, DUALSENSE_STATUS2_USB_PLUGGED, plugged);
    if (_inputReport.status2 != newStatus2) {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.status2 = newStatus2;
    }
    if (_config->getAutoReport()) sendReport();
}

void DualsenseEdgeGamepadDevice::setAuthNonce(const uint8_t* data, size_t len) {
    if (_authF1) _authF1->setValue(data, len);
}

void DualsenseEdgeGamepadDevice::setAuthSigningState(const uint8_t* data, size_t len) {
    if (_authF2) _authF2->setValue(data, len);
}

void DualsenseEdgeGamepadDevice::buildFeatureReportWithCrc(
    uint8_t reportId, const uint8_t* payload, size_t payloadSize,
    uint8_t* outBuffer, size_t outSize)
{
    if (outSize < payloadSize + 4) return;
    memcpy(outBuffer, payload, payloadSize);
    uint8_t hdr[] = { PS_FEATURE_CRC32_SEED, reportId };
    uint32_t crc = crc32_le(0xFFFFFFFF, hdr, sizeof(hdr));
    crc = ~crc32_le(crc, outBuffer, payloadSize);
    outBuffer[payloadSize + 0] = (uint8_t)( crc        & 0xFF);
    outBuffer[payloadSize + 1] = (uint8_t)((crc >>  8) & 0xFF);
    outBuffer[payloadSize + 2] = (uint8_t)((crc >> 16) & 0xFF);
    outBuffer[payloadSize + 3] = (uint8_t)((crc >> 24) & 0xFF);
}

void DualsenseEdgeGamepadDevice::populateFeatureReportOnRead(NimBLECharacteristic* pCharacteristic)
{
    ESP_LOGD(LOG_TAG, "populateFeatureReportOnRead pChar=%p", (void*)pCharacteristic);

    if (pCharacteristic == getOutput()) {
        ESP_LOGI(LOG_TAG, "Host reading output report — providing default");
        // Return a valid but empty output report (DS_OUTPUT_REPORT_BT_SIZE bytes for BLE format)
        static uint8_t defaultOut[DS_OUTPUT_REPORT_BT_SIZE] = {0};
        defaultOut[0] = 0x31;  // Report ID
        defaultOut[2] = 0x10;  // Tag (DS_OUTPUT_TAG)
        std::lock_guard<std::mutex> lock(_mutex);
        pCharacteristic->setValue(defaultOut, DS_OUTPUT_REPORT_BT_SIZE);
    } else if (pCharacteristic == _calibration) {
        ESP_LOGD(LOG_TAG, "Host reading calibration");
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
    } else if (pCharacteristic == _firmwareInfo) {
        ESP_LOGI(LOG_TAG, "Host reading firmware info");
        std::lock_guard<std::mutex> lock(_mutex);
        uint8_t buf[DUALSENSE_FIRMWARE_INFO_REPORT_SIZE] = {};
        buildFeatureReportWithCrc(DUALSENSE_FIRMWARE_INFO_REPORT_ID,
            DualsenseEdge_FirmwareInfo, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE - 5,
            buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
        pCharacteristic->setValue(buf, DUALSENSE_FIRMWARE_INFO_REPORT_SIZE);
    } else if (pCharacteristic == _pairingInfo) {
        ESP_LOGI(LOG_TAG, "Host reading pairing info");
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_BT);
        memcpy(_pairingReport.mac_address, mac, 6);
        memcpy(_pairingReport.common, (uint8_t*)&DualsenseEdge_PairInfo_common, 9);
        uint8_t bthdr[] = { PS_FEATURE_CRC32_SEED, DUALSENSE_PAIRING_INFO_REPORT_ID };
        uint32_t crc = crc32_le(0xFFFFFFFF, bthdr, sizeof(bthdr));
        crc = ~crc32_le(crc, (uint8_t*)&_pairingReport, offsetof(DualsenseGamepadPairingReportdata, crc32));
        _pairingReport.crc32 = crc;
        std::lock_guard<std::mutex> lock(_mutex);
        pCharacteristic->setValue((uint8_t*)&_pairingReport, DUALSENSE_PAIRING_INFO_REPORT_SIZE);
    } else if (pCharacteristic == _btPatchInfo) {
        ESP_LOGI(LOG_TAG, "Host reading BT patch info");
        std::lock_guard<std::mutex> lock(_mutex);
        uint8_t buf[DUALSENSE_BT_PATCH_REPORT_SIZE];
        uint8_t payload[DUALSENSE_BT_PATCH_REPORT_SIZE - 4] = { 0 };
        buildFeatureReportWithCrc(DUALSENSE_BT_PATCH_REPORT_ID,
            payload, sizeof(payload), buf, DUALSENSE_BT_PATCH_REPORT_SIZE);
        pCharacteristic->setValue(buf, DUALSENSE_BT_PATCH_REPORT_SIZE);
    } else if (pCharacteristic == _authF1) {
        ESP_LOGI(LOG_TAG, "Host reading auth nonce (F1)");
        // Value was pre-set by setAuthNonce(); nothing to regenerate.
    } else if (pCharacteristic == _authF2) {
        ESP_LOGI(LOG_TAG, "Host reading auth signing state (F2)");
        // Value was pre-set by setAuthSigningState(); nothing to regenerate.
    }
}

uint32_t DualsenseEdgeGamepadDevice::crc32_le(unsigned int crc, unsigned char const* buf, unsigned int len)
{
    for (unsigned int i = 0; i < len; i++)
        crc = m_pCrcTable[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

void DualsenseEdgeGamepadDevice::generate_crc_table(uint32_t* crcTable)
{
    const uint32_t POLYNOMIAL = 0xEDB88320; // 0x04C11DB7 reversed
    uint8_t b = 0;
    do {
        uint32_t remainder = b;
        for (unsigned long bit = 8; bit > 0; --bit)
            remainder = (remainder & 1) ? (remainder >> 1) ^ POLYNOMIAL : (remainder >> 1);
        crcTable[(size_t)b] = remainder;
    } while (0 != ++b);
}
