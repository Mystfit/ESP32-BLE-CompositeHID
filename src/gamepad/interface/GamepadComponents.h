#pragma once
#include <stdint.h>
#include <Callback.h>

// =============================================================================
// Common data types
// =============================================================================

struct RGBColor    { uint8_t r, g, b; };
struct RumbleState  { uint8_t weak, strong; };

// PCM audio frame — 31 signed 8-bit stereo samples at 8 kHz.
// Wire format of the SC2026 0x88 output report: [id, 31, L0..L30, R0..R30].
// Both channels are preserved so the receiver can route to I2S, a DAC, a codec,
// or render a waveform — without being coupled to the haptic pad hardware.
struct PCMAudioFrame {
    static constexpr uint8_t SAMPLES = 31;
    int8_t left[SAMPLES];
    int8_t right[SAMPLES];
};

// Parameters carried in a 0x86 (PCM_INIT) output report.
// channel: 0 = left pad, 1 = right pad, 2 = both pads simultaneously.
struct PCMInitParams {
    uint8_t channel;
    uint8_t param;
};

// =============================================================================
// Component interfaces — pure virtual
// =============================================================================

// IButton — a single digital button
struct IButton {
    virtual void press() = 0;
    virtual void release() = 0;
    virtual bool isPressed() const = 0;
    virtual ~IButton() = default;
};

// IAnalogTrigger — analog input that also has a digital threshold state
struct IAnalogTrigger : public IButton {
    virtual void     setValue(uint16_t value) = 0;
    virtual uint16_t getValue()    const = 0;
    virtual uint16_t getMinValue() const = 0;
    virtual uint16_t getMaxValue() const = 0;
    // press() / release() set the value to max / min
};

// IAnalogStick — two-axis analog input
struct IAnalogStick {
    virtual void    setX(int16_t x) = 0;
    virtual void    setY(int16_t y) = 0;
    virtual void    set(int16_t x, int16_t y) = 0;
    virtual int16_t getX()         const = 0;
    virtual int16_t getY()         const = 0;
    virtual int16_t getMinValue()  const = 0;
    virtual int16_t getMaxValue()  const = 0;
    virtual void    center() = 0;
    virtual ~IAnalogStick() = default;
};

// ITouchpad — 2D touch surface
struct ITouchpad {
    virtual void    set(int16_t x, int16_t y) = 0;
    virtual void    release() = 0;
    virtual bool    isTouching() const = 0;
    virtual int16_t getX() const = 0;
    virtual int16_t getY() const = 0;
    virtual ~ITouchpad() = default;
};

enum class DPadDirection : uint8_t {
    NONE = 0,
    N    = 1,
    NE   = 2,
    E    = 3,
    SE   = 4,
    S    = 5,
    SW   = 6,
    W    = 7,
    NW   = 8
};

// IDPad — directional pad.
struct IDPad {
    virtual void          setDirection(DPadDirection direction) = 0;
    virtual void          release() = 0;
    virtual DPadDirection getDirection() const = 0;
    virtual ~IDPad() = default;
};

// ILight — RGB LED / lightbar. Receives color changes from host via Signal.
struct ILight {
    Signal<RGBColor> onColorChanged;
};

// IPlayerIndicator — player number LED(s). Receives value from host via Signal.
struct IPlayerIndicator {
    Signal<uint8_t> onChanged;
};

// IRumbleMotor — haptic rumble. Receives strength commands from host via Signal.
struct IRumbleMotor {
    Signal<RumbleState> onRumble;
};

// IBattery — battery status reported to host (device → host direction)
struct IBattery {
    virtual uint8_t getLevel()    const = 0;   // 0-100%
    virtual bool    isCharging()  const = 0;
    virtual void    setLevel(uint8_t percent) = 0;
    virtual void    setCharging(bool charging) = 0;
    virtual ~IBattery() = default;
};

// ISpeaker — speaker audio output received from host via Signals.
// onVolumeChanged fires when the host sets a volume level.
// onPcmInit    fires when the host sends a PCM_INIT (0x86) command.
// onPcmFrame   fires for each PCM_DATA (0x88) audio frame (31 samples at 8 kHz).
// For SC2026 haptic pads the signals are fired per-pad with the matching channel.
// For the DualSense the PCM path is wired to onPcmFrame once the audio protocol is known.
struct ISpeaker {
    Signal<uint8_t>      onVolumeChanged;
    Signal<PCMInitParams> onPcmInit;
    Signal<PCMAudioFrame> onPcmFrame;
};

// IMicrophone — mic volume and mute received from host via Signal
struct IMicrophone {
    Signal<uint8_t> onVolumeChanged;
    Signal<bool>    onMuteChanged;
};

// =============================================================================
// Concrete implementations
// =============================================================================

// MaskButtonImpl<T> — button backed by a bitmask field of type T
template<typename T>
class MaskButtonImpl final : public IButton {
    T& _field;
    T  _mask;
public:
    MaskButtonImpl(T& field, T mask) : _field(field), _mask(mask) {}
    void press()   override { _field |=  _mask; }
    void release() override { _field &= static_cast<T>(~_mask); }
    bool isPressed() const override { return (_field & _mask) == _mask; }
};

// DSButtonImpl — button backed by the DualSense 3-byte button array.
// The DualSense hat lives in the low nibble of buttons[0], so all button
// mask bits are shifted left 4 on the wire.
class DSButtonImpl final : public IButton {
    uint8_t* _b;
    uint32_t _mask;   // unshifted logical button mask
public:
    DSButtonImpl(uint8_t* b, uint32_t mask) : _b(b), _mask(mask) {}
    void press() override {
        uint32_t s = _mask << 4;
        _b[0] |= (uint8_t)( s        & 0xFF);
        _b[1] |= (uint8_t)((s >>  8) & 0xFF);
        _b[2] |= (uint8_t)((s >> 16) & 0xFF);
    }
    void release() override {
        uint32_t s = _mask << 4;
        _b[0] &= (uint8_t)~( s        & 0xFF);
        _b[1] &= (uint8_t)~((s >>  8) & 0xFF);
        _b[2] &= (uint8_t)~((s >> 16) & 0xFF);
    }
    bool isPressed() const override {
        uint32_t combined = (uint32_t)_b[0]
                          | ((uint32_t)_b[1] << 8)
                          | ((uint32_t)_b[2] << 16);
        uint32_t s = _mask << 4;
        return (combined & s) == s;
    }
};

// SC2026ButtonImpl — button backed by the SC2026 4-byte button array
class SC2026ButtonImpl final : public IButton {
    uint8_t* _b;
    uint32_t _mask;
public:
    SC2026ButtonImpl(uint8_t* b, uint32_t mask) : _b(b), _mask(mask) {}
    void press() override {
        _b[0] |= (uint8_t)( _mask        & 0xFF);
        _b[1] |= (uint8_t)((_mask >>  8) & 0xFF);
        _b[2] |= (uint8_t)((_mask >> 16) & 0xFF);
        _b[3] |= (uint8_t)((_mask >> 24) & 0xFF);
    }
    void release() override {
        _b[0] &= (uint8_t)~( _mask        & 0xFF);
        _b[1] &= (uint8_t)~((_mask >>  8) & 0xFF);
        _b[2] &= (uint8_t)~((_mask >> 16) & 0xFF);
        _b[3] &= (uint8_t)~((_mask >> 24) & 0xFF);
    }
    bool isPressed() const override {
        uint32_t c = (uint32_t)_b[0]
                   | ((uint32_t)_b[1] <<  8)
                   | ((uint32_t)_b[2] << 16)
                   | ((uint32_t)_b[3] << 24);
        return (c & _mask) == _mask;
    }
};

// AnalogTriggerImpl<T> — analog trigger backed by a typed field
template<typename T>
class AnalogTriggerImpl final : public IAnalogTrigger {
    T& _field;
    T  _lo, _hi;
public:
    AnalogTriggerImpl(T& field, T minVal, T maxVal)
        : _field(field), _lo(minVal), _hi(maxVal) {}
    void setValue(uint16_t v) override {
        if ((int32_t)v < (int32_t)_lo) v = (uint16_t)_lo;
        if ((int32_t)v > (int32_t)_hi) v = (uint16_t)_hi;
        _field = (T)v;
    }
    uint16_t getValue()    const override { return (uint16_t)_field; }
    uint16_t getMinValue() const override { return (uint16_t)_lo;    }
    uint16_t getMaxValue() const override { return (uint16_t)_hi;    }
    void press()   override { _field = _hi; }
    void release() override { _field = _lo; }
    bool isPressed() const override { return _field >= _hi; }
};

// AnalogStickImpl<T, Bias> — stick backed by two typed fields.
// Bias is added on write and subtracted on read (e.g., 0x8000 for Xbox, 0x80 for DS).
template<typename T, int32_t Bias = 0>
class AnalogStickImpl final : public IAnalogStick {
    T&      _x;
    T&      _y;
    int16_t _lo, _hi;
public:
    AnalogStickImpl(T& x, T& y, int16_t minVal, int16_t maxVal)
        : _x(x), _y(y), _lo(minVal), _hi(maxVal) {}
    void setX(int16_t x) override {
        if (x < _lo) x = _lo;
        if (x > _hi) x = _hi;
        _x = (T)((int32_t)x + (int32_t)Bias);
    }
    void setY(int16_t y) override {
        if (y < _lo) y = _lo;
        if (y > _hi) y = _hi;
        _y = (T)((int32_t)y + (int32_t)Bias);
    }
    void set(int16_t x, int16_t y) override { setX(x); setY(y); }
    int16_t getX() const override { return (int16_t)((int32_t)_x - (int32_t)Bias); }
    int16_t getY() const override { return (int16_t)((int32_t)_y - (int32_t)Bias); }
    int16_t getMinValue() const override { return _lo; }
    int16_t getMaxValue() const override { return _hi; }
    void center()         override { setX(0); setY(0); }
};

// HatDPadImpl — DPad encoded as a full uint8_t hat value (Xbox).
// Xbox hat encoding matches DPadDirection values exactly (NONE=0, N=1 … NW=8).
class HatDPadImpl final : public IDPad {
    uint8_t& _hat;
    uint8_t  _noneVal;
public:
    HatDPadImpl(uint8_t& hat, uint8_t noneVal) : _hat(hat), _noneVal(noneVal) {}
    void setDirection(DPadDirection d) override { _hat = static_cast<uint8_t>(d); }
    void release() override { _hat = _noneVal; }
    DPadDirection getDirection() const override { return static_cast<DPadDirection>(_hat); }
};

// NibbleHatDPadImpl — DPad encoded in the low nibble of a byte (DualSense buttons[0]).
// DS-internal hat encoding: 0=N, 1=NE, 2=E, 3=SE, 4=S, 5=SW, 6=W, 7=NW, 8=none.
// setDirection/getDirection convert to/from the canonical DPadDirection encoding.
class NibbleHatDPadImpl final : public IDPad {
    uint8_t* _b;
    static constexpr uint8_t DS_NONE = 0x08;
    // DPadDirection (NONE=0,N=1…NW=8) → DS nibble (N=0…NW=7,NONE=8)
    static uint8_t dirToDS(DPadDirection d) {
        static const uint8_t LUT[9] = { 8, 0, 1, 2, 3, 4, 5, 6, 7 };
        uint8_t i = static_cast<uint8_t>(d);
        return i < 9 ? LUT[i] : DS_NONE;
    }
    // DS nibble → DPadDirection
    static DPadDirection dsToDir(uint8_t ds) {
        return ds < 8 ? static_cast<DPadDirection>(ds + 1) : DPadDirection::NONE;
    }
public:
    explicit NibbleHatDPadImpl(uint8_t* b) : _b(b) {}
    void setDirection(DPadDirection d) override {
        *_b = (uint8_t)((*_b & 0xF0) | (dirToDS(d) & 0x0F));
    }
    void release() override { *_b = (uint8_t)((*_b & 0xF0) | DS_NONE); }
    DPadDirection getDirection() const override { return dsToDir(*_b & 0x0F); }
};

// BitmaskDPadImpl — DPad encoded as four individual SC2026ButtonImpl bits
class BitmaskDPadImpl final : public IDPad {
    static constexpr uint8_t F_N = 1, F_E = 2, F_S = 4, F_W = 8;
    SC2026ButtonImpl _up, _down, _left, _right;
public:
    BitmaskDPadImpl(uint8_t* btns,
                    uint32_t upMask, uint32_t downMask,
                    uint32_t leftMask, uint32_t rightMask)
        : _up(btns, upMask), _down(btns, downMask),
          _left(btns, leftMask), _right(btns, rightMask) {}
    void release() override {
        _up.release(); _down.release(); _left.release(); _right.release();
    }
    void setDirection(DPadDirection dir) override {
        static const uint8_t LUT[9] = {
            0,
            F_N, F_N|F_E, F_E, F_S|F_E,
            F_S, F_S|F_W, F_W, F_N|F_W
        };
        uint8_t i = static_cast<uint8_t>(dir);
        uint8_t flags = i < 9 ? LUT[i] : 0;
        release();
        if (flags & F_N) _up.press();
        if (flags & F_S) _down.press();
        if (flags & F_E) _right.press();
        if (flags & F_W) _left.press();
    }
    DPadDirection getDirection() const override {
        bool u = _up.isPressed(), d = _down.isPressed();
        bool l = _left.isPressed(), r = _right.isPressed();
        uint8_t flags = (u ? F_N : 0) | (d ? F_S : 0)
                      | (r ? F_E : 0) | (l ? F_W : 0);
        switch (flags) {
            case F_N:       return DPadDirection::N;
            case F_N|F_E:   return DPadDirection::NE;
            case F_E:       return DPadDirection::E;
            case F_S|F_E:   return DPadDirection::SE;
            case F_S:       return DPadDirection::S;
            case F_S|F_W:   return DPadDirection::SW;
            case F_W:       return DPadDirection::W;
            case F_N|F_W:   return DPadDirection::NW;
            default:        return DPadDirection::NONE;
        }
    }
};
