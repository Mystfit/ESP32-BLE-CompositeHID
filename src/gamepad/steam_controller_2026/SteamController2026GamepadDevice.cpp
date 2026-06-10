#include "SteamController2026GamepadDevice.h"
#include "BleCompositeHID.h"
#include "ArduinoDefines.h"
#include <cstdint>
#include <string.h>

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "SC2026GamepadDevice"
#else
#include "esp_log.h"
static const char* LOG_TAG = "SC2026GamepadDevice";
#endif

// ---- SteamController2026GamepadCallbacks ----

SteamController2026GamepadCallbacks::SteamController2026GamepadCallbacks(SteamController2026GamepadDevice* device)
    : _device(device)
{}

void SteamController2026GamepadCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    [[maybe_unused]] uint16_t handle = pCharacteristic->getHandle();
    const NimBLEAttValue& value = pCharacteristic->getValue();
    size_t len = value.size();

    if (len == 0) {
        ESP_LOGD(LOG_TAG, "onWrite: handle=%d size=0", handle);
        return;
    }
    ESP_LOGD(LOG_TAG, "onWrite: handle=%d size=%d", handle, len);
    ESP_LOG_BUFFER_HEX_LEVEL(LOG_TAG, value.data(), len, ESP_LOG_DEBUG);

    if (!(_device && pCharacteristic == _device->getOutputChar()))
        return;

    const uint8_t* d = value.data();

    switch (d[0]) {
        case SC2026_OUT_PCM_INIT: {
            // [0x86, 0x02, ch, param] — put touchpad channel into PCM streaming mode.
            // Rumble actuator channels (3/4/5) have no speaker and are ignored here.
            if (len < 4) break;
            PCMInitParams p{ d[2], d[3] };
            switch (p.channel) {
                case SC2026_CH_LPAD:  _device->leftPad().speaker.onPcmInit.fire(p);  break;
                case SC2026_CH_RPAD:  _device->rightPad().speaker.onPcmInit.fire(p); break;
                case SC2026_CH_BOTH_PADS:
                    _device->leftPad().speaker.onPcmInit.fire(p);
                    _device->rightPad().speaker.onPcmInit.fire(p);
                    break;
                default: break;
            }
            break;
        }
        case SC2026_OUT_PCM_DATA: {
            // [0x88, 31, L0..L30, R0..R30] — 31+31 signed 8-bit samples at 8 kHz.
            // L channel → left touchpad speaker, R channel → right touchpad speaker.
            if (len < 2 + 2 * PCMAudioFrame::SAMPLES) break;
            PCMAudioFrame lFrame, rFrame;
            memcpy(lFrame.left,  d + 2,                          PCMAudioFrame::SAMPLES);
            memcpy(lFrame.right, d + 2,                          PCMAudioFrame::SAMPLES);
            memcpy(rFrame.left,  d + 2 + PCMAudioFrame::SAMPLES, PCMAudioFrame::SAMPLES);
            memcpy(rFrame.right, d + 2 + PCMAudioFrame::SAMPLES, PCMAudioFrame::SAMPLES);
            _device->leftPad().speaker.onPcmFrame.fire(lFrame);
            _device->rightPad().speaker.onPcmFrame.fire(rFrame);
            break;
        }
        case SC2026_OUT_HAPTIC_STOP: {
            // [0x82, ch] — stop the actuator on channel ch.
            if (len < 2) break;
            uint8_t ch = d[1];
            switch (ch) {
                case SC2026_CH_LPAD:        _device->leftPad().onStop.fire(ch);        break;
                case SC2026_CH_RPAD:        _device->rightPad().onStop.fire(ch);       break;
                case SC2026_CH_BOTH_PADS:
                    _device->leftPad().onStop.fire(ch);
                    _device->rightPad().onStop.fire(ch);
                    break;
                case SC2026_CH_LRUMBLE:     _device->leftActuator().onStop.fire(ch);   break;
                case SC2026_CH_RRUMBLE:     _device->rightActuator().onStop.fire(ch);  break;
                case SC2026_CH_BOTH_RUMBLE:
                    _device->leftActuator().onStop.fire(ch);
                    _device->rightActuator().onStop.fire(ch);
                    break;
                default: break;
            }
            break;
        }
        case SC2026_OUT_HAPTIC_PLAY: {
            // [0x83, ch, amp, freq_lo, freq_hi, 0xFF, duration, 0, 0, 0]
            if (len < 7) break;
            SC2026HapticPlay p;
            p.channel   = d[1];
            p.amplitude = d[2];
            p.frequency = static_cast<uint16_t>(d[3]) | (static_cast<uint16_t>(d[4]) << 8);
            p.duration  = d[6];
            switch (p.channel) {
                case SC2026_CH_LPAD:        _device->leftPad().onPlay.fire(p);        break;
                case SC2026_CH_RPAD:        _device->rightPad().onPlay.fire(p);       break;
                case SC2026_CH_BOTH_PADS:
                    _device->leftPad().onPlay.fire(p);
                    _device->rightPad().onPlay.fire(p);
                    break;
                case SC2026_CH_LRUMBLE:     _device->leftActuator().onPlay.fire(p);   break;
                case SC2026_CH_RRUMBLE:     _device->rightActuator().onPlay.fire(p);  break;
                case SC2026_CH_BOTH_RUMBLE:
                    _device->leftActuator().onPlay.fire(p);
                    _device->rightActuator().onPlay.fire(p);
                    break;
                default: break;
            }
            break;
        }
        default:
            break;
    }
}

void SteamController2026GamepadCallbacks::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    ESP_LOGD(LOG_TAG, "onRead: handle=%d", pCharacteristic->getHandle());
}

void SteamController2026GamepadCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue)
{
    ESP_LOGI(LOG_TAG, "onSubscribe: handle=%d subValue=%d", pCharacteristic->getHandle(), subValue);
}

void SteamController2026GamepadCallbacks::onStatus(NimBLECharacteristic* pCharacteristic, int code)
{
    ESP_LOGD(LOG_TAG, "onStatus: code=%d", code);
}

// ---- SteamController2026GamepadDevice ----

SteamController2026GamepadDevice::SteamController2026GamepadDevice() :
    _inputReport{},
    _a             (_inputReport.btns, SC2026_BTN_A),
    _b             (_inputReport.btns, SC2026_BTN_B),
    _x             (_inputReport.btns, SC2026_BTN_X),
    _y             (_inputReport.btns, SC2026_BTN_Y),
    _quickAccess   (_inputReport.btns, SC2026_BTN_QUICK_ACCESS),
    _menu          (_inputReport.btns, SC2026_BTN_MENU),
    _r4            (_inputReport.btns, SC2026_BTN_R4),
    _r5            (_inputReport.btns, SC2026_BTN_R5),
    _r1            (_inputReport.btns, SC2026_BTN_R1),
    _l4            (_inputReport.btns, SC2026_BTN_L4),
    _l5            (_inputReport.btns, SC2026_BTN_L5),
    _l1            (_inputReport.btns, SC2026_BTN_L1),
    _steam         (_inputReport.btns, SC2026_BTN_STEAM),
    _view          (_inputReport.btns, SC2026_BTN_VIEW),
    _lstickClick   (_inputReport.btns, SC2026_BTN_LSTICK_CLICK),
    _rstickClick   (_inputReport.btns, SC2026_BTN_RSTICK_CLICK),
    _lpadClick     (_inputReport.btns, SC2026_BTN_LPAD_CLICK),
    _rpadClick     (_inputReport.btns, SC2026_BTN_RPAD_CLICK),
    _lpadTouch     (_inputReport.btns, SC2026_BTN_LPAD_TOUCH),
    _rpadTouch     (_inputReport.btns, SC2026_BTN_RPAD_TOUCH),
    _lstickCap     (_inputReport.btns, SC2026_BTN_LSTICK_CAP),
    _rstickCap     (_inputReport.btns, SC2026_BTN_RSTICK_CAP),
    _lgripCap      (_inputReport.btns, SC2026_BTN_LGRIP_CAP),
    _rgripCap      (_inputReport.btns, SC2026_BTN_RGRIP_CAP),
    _leftTrigger  (_inputReport.ltrigger, SC2026_TRIGGER_MIN, SC2026_TRIGGER_MAX),
    _rightTrigger (_inputReport.rtrigger, SC2026_TRIGGER_MIN, SC2026_TRIGGER_MAX),
    _leftStick    (_inputReport.left_stick_x,  _inputReport.left_stick_y,
                  SC2026_AXIS_MIN, SC2026_AXIS_MAX),
    _rightStick   (_inputReport.right_stick_x, _inputReport.right_stick_y,
                  SC2026_AXIS_MIN, SC2026_AXIS_MAX),
    _leftPad  (_inputReport.lpad_x, _inputReport.lpad_y, _inputReport.lpad_force,
               _inputReport.btns, SC2026_BTN_LPAD_TOUCH, SC2026_BTN_LPAD_CLICK),
    _rightPad (_inputReport.rpad_x, _inputReport.rpad_y, _inputReport.rpad_force,
               _inputReport.btns, SC2026_BTN_RPAD_TOUCH, SC2026_BTN_RPAD_CLICK),
    _dpad       (_inputReport.btns,
                 SC2026_BTN_DPAD_UP, SC2026_BTN_DPAD_DOWN,
                 SC2026_BTN_DPAD_LEFT, SC2026_BTN_DPAD_RIGHT),
    _config   (new SteamController2026DeviceConfiguration()),
    _callbacks(nullptr)
{
}

SteamController2026GamepadDevice::SteamController2026GamepadDevice(SteamController2026DeviceConfiguration* config) :
    _inputReport{},
    _a             (_inputReport.btns, SC2026_BTN_A),
    _b             (_inputReport.btns, SC2026_BTN_B),
    _x             (_inputReport.btns, SC2026_BTN_X),
    _y             (_inputReport.btns, SC2026_BTN_Y),
    _quickAccess   (_inputReport.btns, SC2026_BTN_QUICK_ACCESS),
    _menu          (_inputReport.btns, SC2026_BTN_MENU),
    _r4            (_inputReport.btns, SC2026_BTN_R4),
    _r5            (_inputReport.btns, SC2026_BTN_R5),
    _r1            (_inputReport.btns, SC2026_BTN_R1),
    _l4            (_inputReport.btns, SC2026_BTN_L4),
    _l5            (_inputReport.btns, SC2026_BTN_L5),
    _l1            (_inputReport.btns, SC2026_BTN_L1),
    _steam         (_inputReport.btns, SC2026_BTN_STEAM),
    _view          (_inputReport.btns, SC2026_BTN_VIEW),
    _lstickClick   (_inputReport.btns, SC2026_BTN_LSTICK_CLICK),
    _rstickClick   (_inputReport.btns, SC2026_BTN_RSTICK_CLICK),
    _lpadClick     (_inputReport.btns, SC2026_BTN_LPAD_CLICK),
    _rpadClick     (_inputReport.btns, SC2026_BTN_RPAD_CLICK),
    _lpadTouch     (_inputReport.btns, SC2026_BTN_LPAD_TOUCH),
    _rpadTouch     (_inputReport.btns, SC2026_BTN_RPAD_TOUCH),
    _lstickCap     (_inputReport.btns, SC2026_BTN_LSTICK_CAP),
    _rstickCap     (_inputReport.btns, SC2026_BTN_RSTICK_CAP),
    _lgripCap      (_inputReport.btns, SC2026_BTN_LGRIP_CAP),
    _rgripCap      (_inputReport.btns, SC2026_BTN_RGRIP_CAP),
    _leftTrigger  (_inputReport.ltrigger, SC2026_TRIGGER_MIN, SC2026_TRIGGER_MAX),
    _rightTrigger (_inputReport.rtrigger, SC2026_TRIGGER_MIN, SC2026_TRIGGER_MAX),
    _leftStick    (_inputReport.left_stick_x,  _inputReport.left_stick_y,
                  SC2026_AXIS_MIN, SC2026_AXIS_MAX),
    _rightStick   (_inputReport.right_stick_x, _inputReport.right_stick_y,
                  SC2026_AXIS_MIN, SC2026_AXIS_MAX),
    _leftPad  (_inputReport.lpad_x, _inputReport.lpad_y, _inputReport.lpad_force,
               _inputReport.btns, SC2026_BTN_LPAD_TOUCH, SC2026_BTN_LPAD_CLICK),
    _rightPad (_inputReport.rpad_x, _inputReport.rpad_y, _inputReport.rpad_force,
               _inputReport.btns, SC2026_BTN_RPAD_TOUCH, SC2026_BTN_RPAD_CLICK),
    _dpad       (_inputReport.btns,
                 SC2026_BTN_DPAD_UP, SC2026_BTN_DPAD_DOWN,
                 SC2026_BTN_DPAD_LEFT, SC2026_BTN_DPAD_RIGHT),
    _config   (config),
    _callbacks(nullptr)
{
}

SteamController2026GamepadDevice::~SteamController2026GamepadDevice()
{
    if (getOutput() && _callbacks) {
        getOutput()->setCallbacks(nullptr);
        delete _callbacks;
        _callbacks = nullptr;
    }
    if (_config) { delete _config; _config = nullptr; }
}

void SteamController2026GamepadDevice::init(NimBLEHIDDevice* hid)
{
    NimBLECharacteristic* input  = hid->getInputReport(SC2026_INPUT_REPORT_ID);
    NimBLECharacteristic* output = hid->getOutputReport(SC2026_OUTPUT_REPORT_ID);
    _callbacks = new SteamController2026GamepadCallbacks(this);
    input->setCallbacks(_callbacks);
    output->setCallbacks(_callbacks);
    setCharacteristics(input, output);
    ESP_LOGI(LOG_TAG, "Characteristics: input=%d output=%d",
        input->getHandle(), output->getHandle());
}

const BaseCompositeDeviceConfiguration* SteamController2026GamepadDevice::getDeviceConfig() const
{
    return _config;
}

void SteamController2026GamepadDevice::resetInputs()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _inputReport = SteamController2026InputReport{};
}

void SteamController2026GamepadDevice::setAccel(int16_t x, int16_t y, int16_t z)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.acc_x = x;
        _inputReport.acc_y = y;
        _inputReport.acc_z = z;
    }
    if (_config->getAutoReport()) sendReport();
}

void SteamController2026GamepadDevice::setGyro(int16_t x, int16_t y, int16_t z)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.gyro_x = x;
        _inputReport.gyro_y = y;
        _inputReport.gyro_z = z;
    }
    if (_config->getAutoReport()) sendReport();
}

void SteamController2026GamepadDevice::setQuaternion(int16_t w, int16_t x, int16_t y, int16_t z)
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.quat_w = w;
        _inputReport.quat_x = x;
        _inputReport.quat_y = y;
        _inputReport.quat_z = z;
    }
    if (_config->getAutoReport()) sendReport();
}

void SteamController2026GamepadDevice::sendReport(bool defer)
{
    if (defer || _config->getAutoDefer())
        queueDeferredReport([this]() { sendGamepadReportImpl(); });
    else
        sendGamepadReportImpl();
}

void SteamController2026GamepadDevice::sendGamepadReportImpl()
{
    auto* input  = getInput();
    auto* parent = getParent();
    if (!input || !parent || !parent->isConnected())
        return;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _inputReport.seq++;
        _inputReport.counter++;
        ESP_LOGD(LOG_TAG, "Sending SC2026 report seq=%d", _inputReport.seq);
        input->setValue(reinterpret_cast<uint8_t*>(&_inputReport), sizeof(_inputReport));
    }
    input->notify();
}
