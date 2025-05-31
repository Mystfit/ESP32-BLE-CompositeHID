#ifndef XBOX_GAMEPAD_DEVICE_H
#define XBOX_GAMEPAD_DEVICE_H

#include <NimBLECharacteristic.h>
#include <Callback.h>
#include <mutex>

#include "BLEHostConfiguration.h"
#include "BaseCompositeDevice.h"
#include "GamepadDevice.h"
#include "XboxDescriptors.h"
#include "XboxGamepadConfiguration.h"

enum class XboxButtons : uint16_t {
    A = 0x01,
    B = 0x02,
    // UNUSED = 0x04,
    X = 0x08,
    Y = 0x10,
    // UNUSED = 0x20,
    LB = 0x40,
    RB = 0x80,
    // UNUSED = 0x100,
    // UNUSED = 0x200,
    SELECT = 0x400,
    START = 0x800,
    HOME = 0x1000,
    LS = 0x2000,
    RS = 0x4000,
    SHARE = 0x8000 // Share button lives in its own byte at the end of the input report but it's available here for convience
};


// Select bitmask
// The share button lives in its own byte at the end of the input report
#define XBOX_BUTTON_SHARE 0x01

// Dpad bitflags
enum XboxDpadFlags : uint8_t {
    NONE = 0x00,
    NORTH = 0x01,
    EAST = 0x02,
    NORTHEAST = 0x03, // NORTH | EAST
    SOUTH = 0x04,
    SOUTHEAST = 0x06, // SOUTH | EAST
    WEST = 0x08,
    NORTHWEST = 0x09, // NORTH | WEST
    SOUTHWEST = 0x0C, // SOUTH | WEST
};

// Trigger range
#define XBOX_TRIGGER_MIN 0
#define XBOX_TRIGGER_MAX 1023

// Thumbstick range
#define XBOX_STICK_MIN -32768
#define XBOX_STICK_MAX 32767

#define XBOX_AXIS_CENTER_OFFSET 0x8000


// Forwards
class XboxGamepadDevice;


class XboxGamepadCallbacks : public NimBLECharacteristicCallbacks
{
public:
    XboxGamepadCallbacks(XboxGamepadDevice* device);

    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;

private:
    XboxGamepadDevice* _device;
};

struct XboxGamepadOutputReportData {
    uint8_t dcEnableActuators = 0x00;   // 4bits for DC Enable Actuators, 4bits padding
    uint8_t leftTriggerMagnitude = 0;
    uint8_t rightTriggerMagnitude = 0; 
    uint8_t weakMotorMagnitude = 0;
    uint8_t strongMotorMagnitude = 0; 
    uint8_t duration = 0;               // UNUSED
    uint8_t startDelay = 0;             // UNUSED
    uint8_t loopCount = 0;              // UNUSED

    constexpr XboxGamepadOutputReportData(uint64_t value = 0) noexcept : 
        dcEnableActuators((value & 0xFF)),
        leftTriggerMagnitude((value >> 8) & 0xFF),
        rightTriggerMagnitude((value >> 16) & 0xFF),
        weakMotorMagnitude((value >> 24) & 0xFF),
        strongMotorMagnitude((value >> 32) & 0xFF),
        duration((value >> 40) & 0xFF),
        startDelay((value >> 48) & 0xFF),
        loopCount((value >> 56) & 0xFF)
    {}
};

#pragma pack(push, 1)
struct XboxGamepadInputReportData {
    uint16_t x = 0;             // Left joystick X
    uint16_t y = 0;             // Left joystick Y
    uint16_t z = 0;             // Right jostick X
    uint16_t rz = 0;            // Right joystick Y
    uint16_t brake = 0;         // 10 bits for brake (left trigger) + 6 bit padding (2 bytes)
    uint16_t accelerator = 0;   // 10 bits for accelerator (right trigger) + 6bit padding
    uint8_t hat = 0x00;         // 4bits for hat switch (Dpad) + 4 bit padding (1 byte) 
    uint16_t buttons = 0x00;    // 15 * 1bit for buttons + 1 bit padding (2 bytes)
    uint8_t share = 0x00;      // 1 bits for share/menu button + 7 bit padding (1 byte)
};
#pragma pack(pop)


static String dPadDirectionName(uint8_t direction){
    if(direction == XboxDpadFlags::NORTH)
        return "NORTH";
    else if(direction == XboxDpadFlags::NORTHEAST)
        return "NORTHEAST";
    else if(direction == XboxDpadFlags::EAST)
        return "EAST";
    else if(direction == XboxDpadFlags::SOUTHEAST)
        return "SOUTHEAST";
    else if(direction == XboxDpadFlags::SOUTH)
        return "SOUTH";
    else if(direction == XboxDpadFlags::SOUTHWEST)
        return "SOUTHWEST";
    else if(direction == XboxDpadFlags::WEST)
        return "WEST";
    else if(direction == XboxDpadFlags::NORTHWEST)
        return "NORTHWEST";
    return "NONE";
}


class XboxGamepadDevice : public BaseCompositeDevice {
public:
    XboxGamepadDevice();
    ~XboxGamepadDevice();

    void setConfig(const XboxGamepadDeviceConfiguration& config);

    void init(NimBLEHIDDevice* hid) override;
    const BaseCompositeDeviceConfiguration& getDeviceConfig() const override;

    Signal<XboxGamepadOutputReportData> onVibrate;

    // Input Controls
    void resetInputs();
    void pressButton(XboxButtons button);    
    void releaseButton(XboxButtons button); 
    bool isButtonPressed(XboxButtons button);
    void setLeftThumbstick(int16_t x = 0, int16_t y = 0);
    void setRightThumbstick(int16_t z = 0, int16_t rZ = 0);
    int16_t getThumbstickMin() const { return XBOX_STICK_MIN; }
    int16_t getThumbstickMax() const { return XBOX_STICK_MAX; }
    void setLeftTrigger(uint16_t rX = 0);
    void setRightTrigger(uint16_t rY = 0);
    uint16_t getTriggerMin() const { return XBOX_TRIGGER_MIN; }
    uint16_t getTriggerMax() const { return XBOX_TRIGGER_MAX; }
    void pressDPad(XboxDpadFlags direction);
    void releaseDPad();
    XboxDpadFlags getDPadPressedDirection() const;
    
    void sendGamepadReport(bool defer = false);

private:
    void sendGamepadReportImpl();

    void pressShare();
    void releaseShare();
    bool isSharePressed();
    XboxGamepadInputReportData _inputReport;

    NimBLECharacteristic* _extra_input;
    XboxGamepadCallbacks* _callbacks;
    const XboxGamepadDeviceConfiguration* _config;

    // Threading
    std::mutex _mutex;
};

#endif // XBOX_GAMEPAD_DEVICE_H
