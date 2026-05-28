#include "XboxGamepadDevice.h"
#include "BleCompositeHID.h"
#include "ArduinoDefines.h"

#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "XboxGamepadDevice"
#else
#include "esp_log.h"
static const char* LOG_TAG = "XboxGamepadDevice";
#endif

// ---- XboxGamepadCallbacks ----

XboxGamepadCallbacks::XboxGamepadCallbacks(XboxGamepadDevice* device) : _device(device) {}

void XboxGamepadCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    // An example packet we might receive from XInput might look like 0x0300002500ff00ff
    XboxGamepadOutputReportData data = pCharacteristic->getValue<uint64_t>();

    ESP_LOGD(LOG_TAG, "onWrite — DC:%d weak:%d strong:%d dur:%d delay:%d loops:%d",
        data.dcEnableActuators, data.weakMotorMagnitude, data.strongMotorMagnitude,
        data.duration, data.startDelay, data.loopCount);

    _device->rumble().onRumble.fire({data.weakMotorMagnitude, data.strongMotorMagnitude});

    if (data.dcEnableActuators)
        _device->playerIndicator().onChanged.fire(data.dcEnableActuators);
}

void XboxGamepadCallbacks::onRead(NimBLECharacteristic*, NimBLEConnInfo&)
{
    ESP_LOGD(LOG_TAG, "onRead");
}

void XboxGamepadCallbacks::onSubscribe(NimBLECharacteristic*, NimBLEConnInfo&, uint16_t)
{
    ESP_LOGD(LOG_TAG, "onSubscribe");
}

void XboxGamepadCallbacks::onStatus(NimBLECharacteristic*, int code)
{
    ESP_LOGD(LOG_TAG, "onStatus %d", code);
}

// ---- XboxGamepadDevice ----

XboxGamepadDevice::XboxGamepadDevice() :
    _inputReport{},
    _a        (_inputReport.buttons, XBOX_BUTTON_A),
    _b        (_inputReport.buttons, XBOX_BUTTON_B),
    _x        (_inputReport.buttons, XBOX_BUTTON_X),
    _y        (_inputReport.buttons, XBOX_BUTTON_Y),
    _lb       (_inputReport.buttons, XBOX_BUTTON_LB),
    _rb       (_inputReport.buttons, XBOX_BUTTON_RB),
    _select   (_inputReport.buttons, XBOX_BUTTON_SELECT),
    _start    (_inputReport.buttons, XBOX_BUTTON_START),
    _home     (_inputReport.buttons, XBOX_BUTTON_HOME),
    _ls       (_inputReport.buttons, XBOX_BUTTON_LS),
    _rs       (_inputReport.buttons, XBOX_BUTTON_RS),
    _share    (_inputReport.share, static_cast<uint8_t>(XBOX_BUTTON_SHARE)),
    _leftTrigger  (_inputReport.brake, XBOX_TRIGGER_MIN, XBOX_TRIGGER_MAX),
    _rightTrigger (_inputReport.accelerator, XBOX_TRIGGER_MIN, XBOX_TRIGGER_MAX),
    _leftStick    (_inputReport.x, _inputReport.y, XBOX_STICK_MIN, XBOX_STICK_MAX),
    _rightStick   (_inputReport.z, _inputReport.rz, XBOX_STICK_MIN, XBOX_STICK_MAX),
    _dpad         (_inputReport.hat, XBOX_BUTTON_DPAD_NONE),
    _extra_input (nullptr),
    _callbacks   (nullptr),
    _config      (new XboxOneSControllerDeviceConfiguration())
{
    _inputReport.x  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.y  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.z  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.rz = XBOX_AXIS_CENTER_OFFSET;
}

XboxGamepadDevice::XboxGamepadDevice(XboxGamepadDeviceConfiguration* config) :
    _inputReport{},
    _a        (_inputReport.buttons, XBOX_BUTTON_A),
    _b        (_inputReport.buttons, XBOX_BUTTON_B),
    _x        (_inputReport.buttons, XBOX_BUTTON_X),
    _y        (_inputReport.buttons, XBOX_BUTTON_Y),
    _lb       (_inputReport.buttons, XBOX_BUTTON_LB),
    _rb       (_inputReport.buttons, XBOX_BUTTON_RB),
    _select   (_inputReport.buttons, XBOX_BUTTON_SELECT),
    _start    (_inputReport.buttons, XBOX_BUTTON_START),
    _home     (_inputReport.buttons, XBOX_BUTTON_HOME),
    _ls       (_inputReport.buttons, XBOX_BUTTON_LS),
    _rs       (_inputReport.buttons, XBOX_BUTTON_RS),
    _share    (_inputReport.share, static_cast<uint8_t>(XBOX_BUTTON_SHARE)),
    _leftTrigger  (_inputReport.brake, XBOX_TRIGGER_MIN, XBOX_TRIGGER_MAX),
    _rightTrigger (_inputReport.accelerator, XBOX_TRIGGER_MIN, XBOX_TRIGGER_MAX),
    _leftStick    (_inputReport.x, _inputReport.y, XBOX_STICK_MIN, XBOX_STICK_MAX),
    _rightStick   (_inputReport.z, _inputReport.rz, XBOX_STICK_MIN, XBOX_STICK_MAX),
    _dpad         (_inputReport.hat, XBOX_BUTTON_DPAD_NONE),
    _extra_input (nullptr),
    _callbacks   (nullptr),
    _config      (config)
{
    _inputReport.x  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.y  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.z  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.rz = XBOX_AXIS_CENTER_OFFSET;
}

XboxGamepadDevice::~XboxGamepadDevice()
{
    if (getOutput() && _callbacks) {
        getOutput()->setCallbacks(nullptr);
        delete _callbacks;
        _callbacks = nullptr;
    }
    if (_config) {
        delete _config;
        _config = nullptr;
    }
}

void XboxGamepadDevice::init(NimBLEHIDDevice* hid)
{
    auto input  = hid->getInputReport(XBOX_INPUT_REPORT_ID);
    auto output = hid->getOutputReport(XBOX_OUTPUT_REPORT_ID);
    _callbacks  = new XboxGamepadCallbacks(this);
    output->setCallbacks(_callbacks);
    setCharacteristics(input, output);
}

const BaseCompositeDeviceConfiguration* XboxGamepadDevice::getDeviceConfig() const
{
    return _config;
}

void XboxGamepadDevice::resetInputs()
{
    std::lock_guard<std::mutex> lock(_mutex);
    memset(&_inputReport, 0, sizeof(XboxGamepadInputReportData));
    _inputReport.x  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.y  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.z  = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.rz = XBOX_AXIS_CENTER_OFFSET;
}

void XboxGamepadDevice::sendReport(bool defer)
{
    if (defer || _config->getAutoDefer()) {
        queueDeferredReport(std::bind(&XboxGamepadDevice::sendGamepadReportImpl, this));
    } else {
        sendGamepadReportImpl();
    }
}

void XboxGamepadDevice::sendGamepadReportImpl()
{
    auto input        = getInput();
    auto parentDevice = getParent();
    if (!input || !parentDevice || !parentDevice->isConnected())
        return;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        ESP_LOGD(LOG_TAG, "Sending gamepad report, size: %d", sizeof(_inputReport));
        input->setValue(reinterpret_cast<uint8_t*>(&_inputReport), sizeof(_inputReport));
    }
    input->notify();
}
