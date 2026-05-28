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
#include "gamepad/interface/GamepadComponents.h"

// Button bitmasks
#define XBOX_BUTTON_A      0x01
#define XBOX_BUTTON_B      0x02
// UNUSED - 0x04
#define XBOX_BUTTON_X      0x08
#define XBOX_BUTTON_Y      0x10
// UNUSED - 0x20
#define XBOX_BUTTON_LB     0x40
#define XBOX_BUTTON_RB     0x80
// UNUSED - 0x100
// UNUSED - 0x200
#define XBOX_BUTTON_SELECT 0x400
#define XBOX_BUTTON_START  0x800
#define XBOX_BUTTON_HOME   0x1000
#define XBOX_BUTTON_LS     0x2000
#define XBOX_BUTTON_RS     0x4000
// The share button lives in its own byte at the end of the input report
#define XBOX_BUTTON_SHARE  0x01

// DPad hat values
#define XBOX_BUTTON_DPAD_NONE      0x00
#define XBOX_BUTTON_DPAD_NORTH     0x01
#define XBOX_BUTTON_DPAD_NORTHEAST 0x02
#define XBOX_BUTTON_DPAD_EAST      0x03
#define XBOX_BUTTON_DPAD_SOUTHEAST 0x04
#define XBOX_BUTTON_DPAD_SOUTH     0x05
#define XBOX_BUTTON_DPAD_SOUTHWEST 0x06
#define XBOX_BUTTON_DPAD_WEST      0x07
#define XBOX_BUTTON_DPAD_NORTHWEST 0x08

enum XboxDpadFlags : uint8_t {
    NONE  = 0x00,
    NORTH = 0x01,
    EAST  = 0x02,
    SOUTH = 0x04,
    WEST  = 0x08
};

#define XBOX_TRIGGER_MIN        0
#define XBOX_TRIGGER_MAX        1023
#define XBOX_STICK_MIN          (-32768)
#define XBOX_STICK_MAX          32767
#define XBOX_AXIS_CENTER_OFFSET 0x8000

// Forwards
class XboxGamepadDevice;

class XboxGamepadCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit XboxGamepadCallbacks(XboxGamepadDevice* device);
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
private:
    XboxGamepadDevice* _device;
};

struct XboxGamepadOutputReportData {
    uint8_t dcEnableActuators    = 0x00;  // 4 bits for DC Enable Actuators, 4 bits padding
    uint8_t leftTriggerMagnitude  = 0;
    uint8_t rightTriggerMagnitude = 0;
    uint8_t weakMotorMagnitude    = 0;
    uint8_t strongMotorMagnitude  = 0;
    uint8_t duration   = 0;  // UNUSED
    uint8_t startDelay = 0;  // UNUSED
    uint8_t loopCount  = 0;  // UNUSED

    constexpr XboxGamepadOutputReportData(uint64_t value = 0) noexcept :
        dcEnableActuators    ((value       ) & 0xFF),
        leftTriggerMagnitude ((value >>  8) & 0xFF),
        rightTriggerMagnitude((value >> 16) & 0xFF),
        weakMotorMagnitude   ((value >> 24) & 0xFF),
        strongMotorMagnitude ((value >> 32) & 0xFF),
        duration             ((value >> 40) & 0xFF),
        startDelay           ((value >> 48) & 0xFF),
        loopCount            ((value >> 56) & 0xFF)
    {}
};

#pragma pack(push, 1)
struct XboxGamepadInputReportData {
    uint16_t x           = 0;  // Left joystick X
    uint16_t y           = 0;  // Left joystick Y
    uint16_t z           = 0;  // Right joystick X
    uint16_t rz          = 0;  // Right joystick Y
    uint16_t brake       = 0;  // 10 bits for brake (left trigger) + 6 bit padding
    uint16_t accelerator = 0;  // 10 bits for accelerator (right trigger) + 6 bit padding
    uint8_t  hat         = 0x00;  // 4 bits for hat switch (Dpad) + 4 bit padding
    uint16_t buttons     = 0x00;  // 15 * 1bit for buttons + 1 bit padding
    uint8_t  share       = 0x00;  // 1 bit for share/menu button + 7 bit padding
};
#pragma pack(pop)

inline uint8_t dPadDirectionToValue(XboxDpadFlags direction) {
    if (direction == XboxDpadFlags::NORTH)                                        return XBOX_BUTTON_DPAD_NORTH;
    if (direction == (XboxDpadFlags)(XboxDpadFlags::EAST | XboxDpadFlags::NORTH)) return XBOX_BUTTON_DPAD_NORTHEAST;
    if (direction == XboxDpadFlags::EAST)                                         return XBOX_BUTTON_DPAD_EAST;
    if (direction == (XboxDpadFlags)(XboxDpadFlags::EAST | XboxDpadFlags::SOUTH)) return XBOX_BUTTON_DPAD_SOUTHEAST;
    if (direction == XboxDpadFlags::SOUTH)                                        return XBOX_BUTTON_DPAD_SOUTH;
    if (direction == (XboxDpadFlags)(XboxDpadFlags::WEST | XboxDpadFlags::SOUTH)) return XBOX_BUTTON_DPAD_SOUTHWEST;
    if (direction == XboxDpadFlags::WEST)                                         return XBOX_BUTTON_DPAD_WEST;
    if (direction == (XboxDpadFlags)(XboxDpadFlags::WEST | XboxDpadFlags::NORTH)) return XBOX_BUTTON_DPAD_NORTHWEST;
    return XBOX_BUTTON_DPAD_NONE;
}

inline String dPadDirectionName(uint8_t direction) {
    if (direction == XBOX_BUTTON_DPAD_NORTH)     return "NORTH";
    if (direction == XBOX_BUTTON_DPAD_NORTHEAST)  return "NORTHEAST";
    if (direction == XBOX_BUTTON_DPAD_EAST)       return "EAST";
    if (direction == XBOX_BUTTON_DPAD_SOUTHEAST)  return "SOUTHEAST";
    if (direction == XBOX_BUTTON_DPAD_SOUTH)      return "SOUTH";
    if (direction == XBOX_BUTTON_DPAD_SOUTHWEST)  return "SOUTHWEST";
    if (direction == XBOX_BUTTON_DPAD_WEST)       return "WEST";
    if (direction == XBOX_BUTTON_DPAD_NORTHWEST)  return "NORTHWEST";
    return "NONE";
}

// =============================================================================
// IXboxGamepad — named interface for Xbox controllers
// =============================================================================
struct IXboxGamepad {
    // Face buttons
    virtual IButton& a() = 0;
    virtual IButton& b() = 0;
    virtual IButton& x() = 0;
    virtual IButton& y() = 0;
    // Shoulder buttons
    virtual IButton& lb() = 0;
    virtual IButton& rb() = 0;
    // Special buttons
    virtual IButton& select() = 0;
    virtual IButton& start() = 0;
    virtual IButton& home() = 0;
    virtual IButton& ls() = 0;   // left stick click
    virtual IButton& rs() = 0;   // right stick click
    virtual IButton& share() = 0;
    // Triggers
    virtual IAnalogTrigger& leftTrigger() = 0;
    virtual IAnalogTrigger& rightTrigger() = 0;
    // Sticks
    virtual IAnalogStick& leftStick() = 0;
    virtual IAnalogStick& rightStick() = 0;
    // DPad
    virtual IDPad& dpad() = 0;
    // Output components (host → device, Signal-based)
    virtual IRumbleMotor&    rumble()          = 0;
    virtual IPlayerIndicator& playerIndicator() = 0;
    // Lifecycle
    virtual void sendReport(bool defer = false) = 0;
    virtual void resetInputs() = 0;
    virtual ~IXboxGamepad() = default;
};

// =============================================================================
// XboxGamepadDevice
// =============================================================================
class XboxGamepadDevice : public BaseCompositeDevice, public IXboxGamepad {
    XboxGamepadInputReportData _inputReport;   // declared first — components hold refs into it

public:
    XboxGamepadDevice();
    explicit XboxGamepadDevice(XboxGamepadDeviceConfiguration* config);
    ~XboxGamepadDevice();

    void init(NimBLEHIDDevice* hid) override;
    const BaseCompositeDeviceConfiguration* getDeviceConfig() const override;

    // IXboxGamepad
    IButton& a()       override { return _a; }
    IButton& b()       override { return _b; }
    IButton& x()       override { return _x; }
    IButton& y()       override { return _y; }
    IButton& lb()      override { return _lb; }
    IButton& rb()      override { return _rb; }
    IButton& select()  override { return _select; }
    IButton& start()   override { return _start; }
    IButton& home()    override { return _home; }
    IButton& ls()      override { return _ls; }
    IButton& rs()      override { return _rs; }
    IButton& share()   override { return _share; }
    IAnalogTrigger& leftTrigger()  override { return _leftTrigger; }
    IAnalogTrigger& rightTrigger() override { return _rightTrigger; }
    IAnalogStick& leftStick()  override { return _leftStick; }
    IAnalogStick& rightStick() override { return _rightStick; }
    IDPad& dpad()               override { return _dpad; }
    IRumbleMotor&    rumble()          override { return _rumble; }
    IPlayerIndicator& playerIndicator() override { return _playerIndicator; }
    void sendReport(bool defer = false) override;
    void resetInputs() override;

private:
    void sendGamepadReportImpl();

    MaskButtonImpl<uint16_t> _a, _b, _x, _y;
    MaskButtonImpl<uint16_t> _lb, _rb;
    MaskButtonImpl<uint16_t> _select, _start, _home;
    MaskButtonImpl<uint16_t> _ls, _rs;
    MaskButtonImpl<uint8_t>  _share;

    AnalogTriggerImpl<uint16_t> _leftTrigger;
    AnalogTriggerImpl<uint16_t> _rightTrigger;

    AnalogStickImpl<uint16_t, 0x8000> _leftStick;
    AnalogStickImpl<uint16_t, 0x8000> _rightStick;

    HatDPadImpl      _dpad;
    IPlayerIndicator _playerIndicator;
    IRumbleMotor     _rumble;

    NimBLECharacteristic*           _extra_input;
    XboxGamepadCallbacks*           _callbacks;
    XboxGamepadDeviceConfiguration* _config;
    std::mutex                      _mutex;
};

#endif // XBOX_GAMEPAD_DEVICE_H
