#include "XboxGamepadDevice.h"
#include "BleCompositeHID.h"


#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "XboxGamepadDevice"
#else
#include "esp_log.h"
static const char *LOG_TAG = "XboxGamepadDevice";
#endif

XboxGamepadCallbacks::XboxGamepadCallbacks(XboxGamepadDevice* device) : _device(device)
{
}

void XboxGamepadCallbacks::onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{    
    // An example packet we might receive from XInput might look like 0x0300002500ff00ff
    XboxGamepadOutputReportData vibrationData = pCharacteristic->getValue<uint64_t>();
    
    ESP_LOGD(LOG_TAG, "XboxGamepadCallbacks::onWrite, Size: %d, DC enable: %d, magnitudeWeak: %d, magnitudeStrong: %d, duration: %d, start delay: %d, loop count: %d", 
        pCharacteristic->getValue().size(),
        vibrationData.dcEnableActuators, 
        vibrationData.weakMotorMagnitude, 
        vibrationData.strongMotorMagnitude, 
        vibrationData.duration, 
        vibrationData.startDelay, 
        vibrationData.loopCount
    );

    _device->onVibrate.fire(vibrationData);
}

void XboxGamepadCallbacks::onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo)
{
    ESP_LOGD(LOG_TAG, "XboxGamepadCallbacks::onRead");
}

void XboxGamepadCallbacks::onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue)
{
    ESP_LOGD(LOG_TAG, "XboxGamepadCallbacks::onSubscribe");
}

void XboxGamepadCallbacks::onStatus(NimBLECharacteristic* pCharacteristic, int code)
{
    ESP_LOGD(LOG_TAG, "XboxGamepadCallbacks::onStatus, code: %d", code);
}

XboxGamepadDevice::XboxGamepadDevice() :
    _extra_input(nullptr),
    _callbacks(nullptr),
    _config(nullptr)
{
}

// XboxGamepadDevice methods
void XboxGamepadDevice::setConfig(const XboxGamepadDeviceConfiguration& config) {
    _config = &config;
}

XboxGamepadDevice::~XboxGamepadDevice() {
    if (getOutput() && _callbacks){
        getOutput()->setCallbacks(nullptr);
        delete _callbacks;
        _callbacks = nullptr;
    }

    if(_extra_input){
        delete _extra_input;
        _extra_input = nullptr;
    }
}

void XboxGamepadDevice::init(NimBLEHIDDevice* hid) {
    /// Create input characteristic to send events to the computer
    auto input = hid->getInputReport(XBOX_INPUT_REPORT_ID);
    //_extra_input = hid->getInputReport(XBOX_EXTRA_INPUT_REPORT_ID);

    // Create output characteristic to handle events coming from the computer
    auto output = hid->getOutputReport(XBOX_OUTPUT_REPORT_ID);
    _callbacks = new XboxGamepadCallbacks(this);
    output->setCallbacks(_callbacks);

    setCharacteristics(input, output);
}

const BaseCompositeDeviceConfiguration& XboxGamepadDevice::getDeviceConfig() const {
    // Return the device configuration
    return *_config;
}

void XboxGamepadDevice::resetInputs() {
    std::lock_guard<std::mutex> lock(_mutex);
    memset(&_inputReport, 0, sizeof(XboxGamepadInputReportData));

    _inputReport.x = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.y = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.z = XBOX_AXIS_CENTER_OFFSET;
    _inputReport.rz = XBOX_AXIS_CENTER_OFFSET;
}

void XboxGamepadDevice::pressButton(XboxButtons button) {
    // Handle share button seperately
    if(button == XboxButtons::SHARE){
        pressShare();
        return;
    }

    // Avoid double presses
    if (!isButtonPressed(button))
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.buttons |= static_cast<uint16_t>(button);
            ESP_LOGD(LOG_TAG, "XboxGamepadDevice::press, button: %d", button);
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::releaseButton(XboxButtons button) {
    if(button == XboxButtons::SHARE){
        releaseShare();
        return;
    }
    
    // Avoid double presses
    if (isButtonPressed(button))
    {   
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.buttons ^= static_cast<uint16_t>(button);
            ESP_LOGD(LOG_TAG, "XboxGamepadDevice::release, button: %d", button);
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

bool XboxGamepadDevice::isButtonPressed(XboxButtons button) {
    if(button == XboxButtons::SHARE){
        return isSharePressed();
    }
    
    std::lock_guard<std::mutex> lock(_mutex);
    uint16_t buttonValue = static_cast<uint16_t>(button);
    return (bool)((_inputReport.buttons & buttonValue) == buttonValue);
}

void XboxGamepadDevice::setLeftThumbstick(int16_t x, int16_t y) {
    x = constrain(x, XBOX_STICK_MIN, XBOX_STICK_MAX);
    y = constrain(y, XBOX_STICK_MIN, XBOX_STICK_MAX);

    if(_inputReport.x != x || _inputReport.y != y){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.x = (uint16_t)(x + XBOX_AXIS_CENTER_OFFSET);
            _inputReport.y = (uint16_t)(y + XBOX_AXIS_CENTER_OFFSET);
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::setRightThumbstick(int16_t z, int16_t rZ) {
    z = constrain(z, XBOX_STICK_MIN, XBOX_STICK_MAX);
    rZ = constrain(rZ, XBOX_STICK_MIN, XBOX_STICK_MAX);

    if(_inputReport.z != z || _inputReport.rz != rZ){
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.z = (uint16_t)(z + XBOX_AXIS_CENTER_OFFSET);
            _inputReport.rz = (uint16_t)(rZ+ XBOX_AXIS_CENTER_OFFSET);
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::setLeftTrigger(uint16_t value) {
    value = constrain(value, XBOX_TRIGGER_MIN, XBOX_TRIGGER_MAX);

    if (_inputReport.brake != value) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.brake = value;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::setRightTrigger(uint16_t value) {
    value = constrain(value, XBOX_TRIGGER_MIN, XBOX_TRIGGER_MAX);

    if (_inputReport.accelerator != value) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.accelerator = value;
        }

        if (_config->getAutoReport()) {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::pressDPad(XboxDpadFlags direction) {
    // Filter opposite button presses
    if((direction & (XboxDpadFlags::NORTH | XboxDpadFlags::SOUTH)) == (XboxDpadFlags::NORTH | XboxDpadFlags::SOUTH)){
        ESP_LOGD(LOG_TAG, "Filtering opposite button presses - up down");
        direction = (XboxDpadFlags)(direction ^ (uint8_t)(XboxDpadFlags::NORTH | XboxDpadFlags::SOUTH));
    }
    if((direction & (XboxDpadFlags::EAST | XboxDpadFlags::WEST)) == (XboxDpadFlags::EAST | XboxDpadFlags::WEST)){
        ESP_LOGD(LOG_TAG, "Filtering opposite button presses - left right");
        direction = (XboxDpadFlags)(direction ^ (uint8_t)(XboxDpadFlags::EAST | XboxDpadFlags::WEST));
    }

    // Avoid double presses
    if (!getDPadPressedDirection() == direction)
    {
        ESP_LOGD(LOG_TAG, "Pressing dpad direction %s", dPadDirectionName(direction).c_str());
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.hat = direction;
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::releaseDPad() {
    pressDPad(XboxDpadFlags::NONE);
}

XboxDpadFlags XboxGamepadDevice::getDPadPressedDirection() const {
    return static_cast<XboxDpadFlags>(_inputReport.hat);
}

void XboxGamepadDevice::pressShare() {
    // Avoid double presses
    if (!(_inputReport.share & XBOX_BUTTON_SHARE))
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.share |= XBOX_BUTTON_SHARE;
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

void XboxGamepadDevice::releaseShare() {
    if (_inputReport.share & XBOX_BUTTON_SHARE)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _inputReport.share ^= XBOX_BUTTON_SHARE;
        }

        if (_config->getAutoReport())
        {
            sendGamepadReport();
        }
    }
}

bool XboxGamepadDevice::isSharePressed() {
    return (bool)(_inputReport.share & XBOX_BUTTON_SHARE);
}

void XboxGamepadDevice::sendGamepadReport(bool defer) {
    if(defer || _config->getAutoDefer()){
        queueDeferredReport(std::bind(&XboxGamepadDevice::sendGamepadReportImpl, this));
    } else {
        sendGamepadReportImpl();
    }
}

void XboxGamepadDevice::sendGamepadReportImpl(){
    auto input = getInput();
    auto parentDevice = this->getParent();

    if (!input || !parentDevice)
        return;

    if(!parentDevice->isConnected())
        return;

    {
        std::lock_guard<std::mutex> lock(_mutex);
        size_t packedSize = sizeof(_inputReport);
        ESP_LOGD(LOG_TAG, "Sending gamepad report, size: %d", packedSize);
        input->setValue((uint8_t*)&_inputReport, packedSize);
    }
    input->notify();
}
