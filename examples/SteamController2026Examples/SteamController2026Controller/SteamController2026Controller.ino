/**
 * SteamController2026Controller.ino
 *
 * Demonstrates the Steam Controller 2026 BLE HID device using the
 * ISteamController2026 component interface.
 *
 * The ESP32 advertises as a Valve Steam Controller (VID 0x28DE / PID 0x1106).
 * Linux hosts running hid-steam.c will recognise the device by VID/PID.
 */

#include <BleGamepad.h>

static SteamController2026GamepadDevice* sc;
static BleCompositeHID ble_hid("Steam Controller", "Valve", 100);

FunctionSlot<SC2026HapticPlay> hapticPlaySlot(onHapticPlay);
FunctionSlot<uint8_t>         hapticStopSlot(onHapticStop);
FunctionSlot<PCMAudioFrame>   pcmSlot(onPcmFrame);

static void onHapticPlay(SC2026HapticPlay p)
{
    Serial.printf("HapticPlay ch=%d amp=%d freq=%dHz dur=%d\n",
                  p.channel, p.amplitude, p.frequency, p.duration);
}

static void onHapticStop(uint8_t ch)
{
    Serial.printf("HapticStop ch=%d\n", ch);
}

static void onPcmFrame(PCMAudioFrame frame)
{
    // Route frame.left / frame.right to I2S, DAC, codec, or waveform display.
    Serial.printf("PCM L[0]=%d R[0]=%d\n", frame.left[0], frame.right[0]);
}

void setup()
{
    Serial.begin(115200);

    auto* config = new SteamController2026DeviceConfiguration();
    sc = new SteamController2026GamepadDevice(config);
    ble_hid.addDevice(sc);

    // Haptic tone play/stop — all four physical channels
    sc->leftPad().onPlay.attach(hapticPlaySlot);
    sc->leftPad().onStop.attach(hapticStopSlot);
    sc->rightPad().onPlay.attach(hapticPlaySlot);
    sc->rightPad().onStop.attach(hapticStopSlot);
    sc->leftActuator().onPlay.attach(hapticPlaySlot);
    sc->leftActuator().onStop.attach(hapticStopSlot);
    sc->rightActuator().onPlay.attach(hapticPlaySlot);
    sc->rightActuator().onStop.attach(hapticStopSlot);

    // PCM audio stream (touchpad haptic channels only)
    sc->leftPad().speaker.onPcmFrame.attach(pcmSlot);
    sc->rightPad().speaker.onPcmFrame.attach(pcmSlot);

    ble_hid.begin(config->getIdealHostConfiguration());

    Serial.println("Waiting for BLE connection...");
    while (!ble_hid.isConnected()) delay(100);
    Serial.println("Connected!");

    sc->resetInputs();
}

void loop()
{
    if (!ble_hid.isConnected()) {
        delay(100);
        return;
    }

    // --- Face button cycle ---
    sc->a().press();  sc->sendReport(); delay(200);
    sc->a().release();
    sc->b().press();  sc->sendReport(); delay(200);
    sc->b().release();
    sc->x().press();  sc->sendReport(); delay(200);
    sc->x().release();
    sc->y().press();  sc->sendReport(); delay(200);
    sc->y().release();

    // --- Trigger sweep ---
    for (uint16_t v = 0; v <= SC2026_TRIGGER_MAX; v += 512) {
        sc->leftTrigger().setValue(v);
        sc->rightTrigger().setValue(SC2026_TRIGGER_MAX - v);
        sc->sendReport();
        delay(10);
    }
    sc->leftTrigger().release();
    sc->rightTrigger().release();

    // --- Stick sweep ---
    for (int16_t v = SC2026_AXIS_MIN; v <= SC2026_AXIS_MAX; v += 1024) {
        sc->leftStick().set(v, v);
        sc->rightStick().set(-v, -v);
        sc->sendReport();
        delay(10);
    }
    sc->leftStick().center();
    sc->rightStick().center();

    // --- Left touchpad tap ---
    sc->leftPad().set(0, 0);
    sc->leftPad().setForce(8000);
    sc->sendReport();
    delay(200);
    sc->leftPad().release();
    sc->sendReport();

    // --- D-pad cycle ---
    sc->dpad().setDirection(DPadDirection::N); sc->sendReport(); delay(150);
    sc->dpad().setDirection(DPadDirection::E); sc->sendReport(); delay(150);
    sc->dpad().setDirection(DPadDirection::S); sc->sendReport(); delay(150);
    sc->dpad().setDirection(DPadDirection::W); sc->sendReport(); delay(150);
    sc->dpad().release();

    // --- Capacitive stick touch ---
    sc->lstickCap().press();
    sc->sendReport();
    delay(200);
    sc->lstickCap().release();

    sc->sendReport();
    delay(500);
}
