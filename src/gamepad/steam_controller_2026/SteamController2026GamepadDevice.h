#ifndef STEAM_CONTROLLER_2026_GAMEPAD_DEVICE_H
#define STEAM_CONTROLLER_2026_GAMEPAD_DEVICE_H

#include <cstdint>
#include <mutex>
#include <Callback.h>
#include <NimBLECharacteristic.h>

#include "BaseCompositeDevice.h"
#include "BLEHostConfiguration.h"
#include "SteamController2026Descriptors.h"
#include "SteamController2026GamepadConfiguration.h"
#include "gamepad/interface/GamepadComponents.h"

// Valve output-report IDs sent by the host to the SC2026 (matches sc_report.h in SCPlusPLus).
static constexpr uint8_t SC2026_OUT_HAPTIC_STOP = 0x82;  // stop haptic motor
static constexpr uint8_t SC2026_OUT_HAPTIC_PLAY = 0x83;  // play haptic tone: [ch, amp, freq_lo, freq_hi, 0xFF, duration, …]
static constexpr uint8_t SC2026_OUT_PCM_INIT    = 0x86;  // enter PCM mode: [0x86, 0x02, ch, param]
static constexpr uint8_t SC2026_OUT_PCM_DATA    = 0x88;  // PCM frame: [0x88, 31, L0..L30, R0..R30] at 8 kHz signed 8-bit

// SC2026 haptic output channels (ch field in haptic / PCM reports).
// Channels 0–2 drive the touchpad haptic actuators; 3–5 drive the heavy LRA/VCA motors.
// Channels 2 and 5 are virtual "both" shortcuts: PCM data is split L→left, R→right.
static constexpr uint8_t SC2026_CH_LPAD        = 0;
static constexpr uint8_t SC2026_CH_RPAD        = 1;
static constexpr uint8_t SC2026_CH_BOTH_PADS   = 2;
static constexpr uint8_t SC2026_CH_LRUMBLE     = 3;
static constexpr uint8_t SC2026_CH_RRUMBLE     = 4;
static constexpr uint8_t SC2026_CH_BOTH_RUMBLE = 5;

// Parsed 0x83 HAPTIC_PLAY command.
// Wire format: [0x83, ch, amp, freq_lo, freq_hi, 0xFF, duration, 0, 0, 0]
struct SC2026HapticPlay {
    uint8_t  channel;    // SC2026_CH_* value from the report
    uint8_t  amplitude;  // 0–255
    uint16_t frequency;  // Hz, little-endian (freq_lo | freq_hi << 8)
    uint8_t  duration;   // duration in host-defined units
};

// Forwards
class SteamController2026GamepadDevice;

class SteamController2026GamepadCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit SteamController2026GamepadCallbacks(SteamController2026GamepadDevice* device);
    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override;
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override;
    void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override;
private:
    SteamController2026GamepadDevice* _device;
};

// IHapticActuator — output-only haptic channel (SC2026 heavy-LRA channels 3/4).
// onPlay  fires when the host sends a 0x83 HAPTIC_PLAY command for this channel.
// onStop  fires when the host sends a 0x82 HAPTIC_STOP; value is the SC2026_CH_* id.
struct IHapticActuator {
    Signal<SC2026HapticPlay> onPlay;
    Signal<uint8_t>          onStop;
};

// IHapticTouchpad — touchpad haptic channel (SC2026 channels 0/1).
// Adds touch-input virtuals on top of the same haptic output signals.
// speaker exposes the PCM audio stream (0x88 / 0x86 reports) for this pad.
struct IHapticTouchpad : public ITouchpad {
    virtual void    setForce(int16_t force) = 0;
    virtual int16_t getForce() const = 0;
    Signal<SC2026HapticPlay> onPlay;
    Signal<uint8_t>          onStop;
    ISpeaker                 speaker;
};

// HapticTouchpadImpl — concrete IHapticTouchpad backed by SC2026 input report fields
class HapticTouchpadImpl final : public IHapticTouchpad {
    int16_t& _x;
    int16_t& _y;
    int16_t& _force;
    SC2026ButtonImpl _touchBtn;
    SC2026ButtonImpl _clickBtn;
public:
    HapticTouchpadImpl(int16_t& x, int16_t& y, int16_t& force,
                        uint8_t* btns, uint32_t touchMask, uint32_t clickMask)
        : _x(x), _y(y), _force(force),
          _touchBtn(btns, touchMask), _clickBtn(btns, clickMask) {}
    void set(int16_t x, int16_t y) override {
        _x = x; _y = y;
        if (_force > 0) _touchBtn.press();
    }
    void setForce(int16_t force) override {
        _force = force;
        if (force > 0) _touchBtn.press(); else _touchBtn.release();
    }
    void release() override {
        _x = 0; _y = 0; _force = 0;
        _touchBtn.release(); _clickBtn.release();
    }
    bool    isTouching() const override { return _touchBtn.isPressed(); }
    int16_t getX()       const override { return _x; }
    int16_t getY()       const override { return _y; }
    int16_t getForce()   const override { return _force; }
    SC2026ButtonImpl& clickButton() { return _clickBtn; }
};

// =============================================================================
// ISteamController2026 — named interface for Steam Controller 2026
// =============================================================================
struct ISteamController2026 {
    // Face buttons
    virtual IButton& a() = 0;
    virtual IButton& b() = 0;
    virtual IButton& x() = 0;
    virtual IButton& y() = 0;
    // System buttons
    virtual IButton& quickAccess() = 0;
    virtual IButton& menu()        = 0;
    virtual IButton& steam()       = 0;
    virtual IButton& view()        = 0;
    // Bumpers
    virtual IButton& l1() = 0;
    virtual IButton& r1() = 0;
    // Back buttons
    virtual IButton& l4() = 0;
    virtual IButton& r4() = 0;
    virtual IButton& l5() = 0;
    virtual IButton& r5() = 0;
    // Stick clicks
    virtual IButton& lstickClick() = 0;
    virtual IButton& rstickClick() = 0;
    // Capacitive sensors
    virtual IButton& lstickCap() = 0;
    virtual IButton& rstickCap() = 0;
    virtual IButton& lgripCap()  = 0;
    virtual IButton& rgripCap()  = 0;
    // Touchpad click / touch
    virtual IButton& lpadClick()  = 0;
    virtual IButton& rpadClick()  = 0;
    virtual IButton& lpadTouch()  = 0;
    virtual IButton& rpadTouch()  = 0;
    // Triggers
    virtual IAnalogTrigger& leftTrigger()  = 0;
    virtual IAnalogTrigger& rightTrigger() = 0;
    // Sticks
    virtual IAnalogStick& leftStick()  = 0;
    virtual IAnalogStick& rightStick() = 0;
    // Haptic touchpads — channels 0 (left) and 1 (right).
    // ch 2 (BOTH_PADS) routes PCM: L → leftPad, R → rightPad.
    virtual IHapticTouchpad& leftPad()  = 0;
    virtual IHapticTouchpad& rightPad() = 0;
    // Haptic actuators — channels 3 (left) and 4 (right). LRA/VCA motors driven by PCM.
    // ch 5 (BOTH_RUMBLE) routes PCM: L → leftActuator, R → rightActuator.
    virtual IHapticActuator& leftActuator()  = 0;
    virtual IHapticActuator& rightActuator() = 0;
    // DPad
    virtual IDPad& dpad() = 0;
    // Lifecycle
    virtual void sendReport(bool defer = false) = 0;
    virtual void resetInputs() = 0;
    virtual ~ISteamController2026() = default;
};

// =============================================================================
// SteamController2026GamepadDevice
// =============================================================================
class SteamController2026GamepadDevice : public BaseCompositeDevice, public ISteamController2026 {
    SteamController2026InputReport _inputReport;   // declared first — components hold refs into it

public:
    SteamController2026GamepadDevice();
    explicit SteamController2026GamepadDevice(SteamController2026DeviceConfiguration* config);
    ~SteamController2026GamepadDevice();

    void init(NimBLEHIDDevice* hid) override;
    const BaseCompositeDeviceConfiguration* getDeviceConfig() const override;

    // ISteamController2026
    IButton& a()            override { return _a; }
    IButton& b()            override { return _b; }
    IButton& x()            override { return _x; }
    IButton& y()            override { return _y; }
    IButton& quickAccess()  override { return _quickAccess; }
    IButton& menu()         override { return _menu; }
    IButton& steam()        override { return _steam; }
    IButton& view()         override { return _view; }
    IButton& l1()           override { return _l1; }
    IButton& r1()           override { return _r1; }
    IButton& l4()           override { return _l4; }
    IButton& r4()           override { return _r4; }
    IButton& l5()           override { return _l5; }
    IButton& r5()           override { return _r5; }
    IButton& lstickClick()  override { return _lstickClick; }
    IButton& rstickClick()  override { return _rstickClick; }
    IButton& lstickCap()    override { return _lstickCap; }
    IButton& rstickCap()    override { return _rstickCap; }
    IButton& lgripCap()     override { return _lgripCap; }
    IButton& rgripCap()     override { return _rgripCap; }
    IButton& lpadClick()    override { return _lpadClick; }
    IButton& rpadClick()    override { return _rpadClick; }
    IButton& lpadTouch()    override { return _lpadTouch; }
    IButton& rpadTouch()    override { return _rpadTouch; }
    IAnalogTrigger& leftTrigger()  override { return _leftTrigger; }
    IAnalogTrigger& rightTrigger() override { return _rightTrigger; }
    IAnalogStick& leftStick()   override { return _leftStick; }
    IAnalogStick& rightStick()  override { return _rightStick; }
    IHapticTouchpad& leftPad()       override { return _leftPad; }
    IHapticTouchpad& rightPad()      override { return _rightPad; }
    IHapticActuator& leftActuator()  override { return _leftActuator; }
    IHapticActuator& rightActuator() override { return _rightActuator; }
    IDPad& dpad()                    override { return _dpad; }
    void sendReport(bool defer = false) override;
    void resetInputs() override;

    // IMU
    void setAccel(int16_t x, int16_t y, int16_t z);
    void setGyro(int16_t x, int16_t y, int16_t z);
    void setQuaternion(int16_t w, int16_t x, int16_t y, int16_t z);

    // Characteristic accessors for callbacks
    NimBLECharacteristic* getInputChar()  { return getInput(); }
    NimBLECharacteristic* getOutputChar() { return getOutput(); }

private:
    void sendGamepadReportImpl();

    SC2026ButtonImpl _a, _b, _x, _y;
    SC2026ButtonImpl _quickAccess, _menu, _steam, _view;
    SC2026ButtonImpl _l1, _r1;
    SC2026ButtonImpl _l4, _r4, _l5, _r5;
    SC2026ButtonImpl _lstickClick, _rstickClick;
    SC2026ButtonImpl _lpadClick, _rpadClick;
    SC2026ButtonImpl _lpadTouch, _rpadTouch;
    SC2026ButtonImpl _lstickCap, _rstickCap;
    SC2026ButtonImpl _lgripCap, _rgripCap;

    AnalogTriggerImpl<uint16_t> _leftTrigger;
    AnalogTriggerImpl<uint16_t> _rightTrigger;

    AnalogStickImpl<int16_t, 0> _leftStick;
    AnalogStickImpl<int16_t, 0> _rightStick;

    HapticTouchpadImpl _leftPad;
    HapticTouchpadImpl _rightPad;
    IHapticActuator    _leftActuator;
    IHapticActuator    _rightActuator;

    BitmaskDPadImpl _dpad;

    SteamController2026DeviceConfiguration* _config;
    SteamController2026GamepadCallbacks*    _callbacks;
    mutable std::mutex                      _mutex;
};

#endif // STEAM_CONTROLLER_2026_GAMEPAD_DEVICE_H
