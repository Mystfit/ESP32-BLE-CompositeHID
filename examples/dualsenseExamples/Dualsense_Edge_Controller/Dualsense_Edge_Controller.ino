#include <BleGamepad.h>
/*
 * DualSense Edge Controller Example
 *
 * This example emulates a Sony DualSense Edge controller over BLE.
 *
 * RUMBLE NOTE (Steam on Windows):
 * On a fresh connection, Steam will NOT send rumble output reports until you open
 *   Steam → Controller Settings → (select the controller) → Calibration and advanced settings
 *   → Gyro Calibration
 * once. After that Steam's "Game Rumble" toggle auto-enables and rumble works for the rest
 * of the session.
 *
 * BLE-REACHABLE HAPTIC PROXY:
 * The adaptive-trigger "Vibration" effect (DS_TRIGGER_EFFECT_VIBRATION, type 0x26)
 * travels inside the standard 0x31 output report and carries frequency + amplitude. You can
 * drive an external LRA/VCA amplifier from those values.
 */

#define CONFIG_BT_NIMBLE_EXT_ADV 1

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif
int ledPin = LED_BUILTIN;
uint8_t playerLEDs = 0x00;
RGBColor ledColor  = {};
uint8_t motor_weak = 0, motor_strong = 0;

DualsenseEdgeGamepadDevice* dualsense;
BleCompositeHID compositeHID("Libresteishon Edge", "YeaSeb", 100);

// FunctionSlots must be global to persist after setup() completes
FunctionSlot<RumbleState> rumbleSlot(OnRumbleEvent);
FunctionSlot<RGBColor>    lightSlot(OnLightEvent);
FunctionSlot<uint8_t>     playerIndicatorSlot(OnPlayerIndicatorEvent);
FunctionSlot<DualsenseGamepadOutputReportData::ParsedTriggerEffect> triggerEffectSlot(OnTriggerEffectEvent);

void OnRumbleEvent(RumbleState state)
{
    motor_weak   = state.weak;
    motor_strong = state.strong;
    digitalWrite(ledPin, (state.weak > 0 || state.strong > 0) ? HIGH : LOW);
    Serial.println("Rumble — weak: " + String(state.weak) + " strong: " + String(state.strong));
}

void OnLightEvent(RGBColor color)
{
    ledColor = color;
    Serial.println("Lightbar R:" + String(color.r) + " G:" + String(color.g) + " B:" + String(color.b));
}

void OnPlayerIndicatorEvent(uint8_t leds)
{
    playerLEDs = leds;
    String msg = "Player LEDs: 0x" + String(leds, HEX) + " —";
    for (int i = 0; i < 5; i++) {
        if (leds & (1 << i)) msg += " LED" + String(i + 1) + "=on";
    }
    Serial.println(msg);
}

String formatTriggerEffect(const DualsenseGamepadOutputReportData::ParsedTriggerEffect& t)
{
    String s;
    switch (t.subtype()) {
        case DS_TRIGGER_SUBTYPE_OFF:       s = "off"; break;
        case DS_TRIGGER_SUBTYPE_FEEDBACK: {
            auto fb = t.asFeedback();
            s = "feedback(start=" + String(fb.start_position) + ",str=" + String(fb.strength()) + ")";
            break;
        }
        case DS_TRIGGER_SUBTYPE_SLOPE_FEEDBACK: {
            auto sl = t.asSlope();
            s = "slope(start=" + String(sl.start_position) + ",end=" + String(sl.end_position)
              + ",s0=" + String(sl.start_strength) + ",s1=" + String(sl.end_strength) + ")";
            break;
        }
        case DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_FEEDBACK: {
            auto mp = t.asMultiPosition();
            s = "multipos(mask=0x" + String(mp.position_mask, HEX) + ")";
            break;
        }
        case DS_TRIGGER_SUBTYPE_WEAPON: {
            auto w = t.asWeapon();
            s = "weapon(start=" + String(w.start_position) + ",end=" + String(w.end_position)
              + ",str=" + String(w.strength) + ")";
            break;
        }
        case DS_TRIGGER_SUBTYPE_VIBRATION: {
            auto v = t.asVibration();
            s = "vibration(amp=" + String(v.amplitude()) + ",freq=" + String(v.frequency) + "Hz)";
            break;
        }
        case DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_VIBRATION: {
            auto mv = t.asMultiVibration();
            s = "multivib(freq=" + String(mv.frequency) + "Hz)";
            break;
        }
        default:
            s = "unknown(0x" + String(t.mode, HEX) + ")";
            break;
    }
    return s;
}

void OnTriggerEffectEvent(DualsenseGamepadOutputReportData::ParsedTriggerEffect effect)
{
    Serial.println("Trigger effect: " + formatTriggerEffect(effect));
}

void setup()
{
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);

    // Use the factory — creates the device, sets up config, and registers with compositeHID
    auto* config = new DualsenseEdgeControllerDeviceConfiguration();
    dualsense = new DualsenseEdgeGamepadDevice(config);
    compositeHID.addDevice(dualsense);

    // Attach per-component event handlers
    dualsense->rumble().onRumble.attach(rumbleSlot);
    dualsense->light().onColorChanged.attach(lightSlot);
    dualsense->playerIndicator().onChanged.attach(playerIndicatorSlot);
    dualsense->leftTrigger().onEffect.attach(triggerEffectSlot);
    dualsense->rightTrigger().onEffect.attach(triggerEffectSlot);

    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
    Serial.println("VID: 0x" + String(hostConfig.getVid(), HEX)
                 + "  PID: 0x" + String(hostConfig.getPid(), HEX));

    compositeHID.begin(hostConfig);

    while (!compositeHID.isConnected()) delay(100);
    delay(280);

    dualsense->sendPairingInfoReport();
    dualsense->sendFirmInfoReport();
    dualsense->sendCalibrationReport();
    dualsense->resetInputs();

    Serial.println("Select test (3-digit number):");
    Serial.println("  0:Cross  1:Circle  2:Square  3:Triangle");
    Serial.println("  4:L1  5:R1  6:L3  7:R3  8:Select  9:Start  10:Home  11:Mute");
    Serial.println("  12-15:DPad(N/E/W/S)  16:L2  17:R2");
    Serial.println("  18-21:LeftStick L/R/D/U  22-25:RightStick L/R/D/U");
    Serial.println("  26:Circle sticks  27:L4  28:R4  29:L5  30:R5");
    Serial.println("  31:Gyro/Accel  32:Touchpad  33:Battery");
    Serial.println("  34:Trigger feedback sweep  35:Status2 flags (headphones/mic/mute/USB)");
}

void loop()
{
    if (!compositeHID.isConnected()) {
        Serial.println("disconnected");
        delay(500);
        return;
    }

    const float STEP = 0.05f;

    dualsense->timestamp();
    dualsense->seq();

    if (Serial.available() < 2) {
        dualsense->sendReport();
        delay(20);
        yield();
        return;
    }

    int selection = Serial.parseInt();
    Serial.println(selection);

    switch (selection) {
        case 0:
            Serial.println("Cross");
            dualsense->cross().press(); delay(500);
            dualsense->cross().release();
            break;
        case 1:
            Serial.println("Circle");
            dualsense->circle().press(); delay(100);
            dualsense->circle().release();
            break;
        case 2:
            Serial.println("Square");
            dualsense->square().press(); delay(200);
            dualsense->square().release();
            break;
        case 3:
            Serial.println("Triangle");
            dualsense->triangle().press(); delay(200);
            dualsense->triangle().release();
            break;
        case 4:
            Serial.println("L1");
            dualsense->l1().press(); delay(200);
            dualsense->l1().release();
            break;
        case 5:
            Serial.println("R1");
            dualsense->r1().press(); delay(200);
            dualsense->r1().release();
            break;
        case 6:
            Serial.println("L3");
            dualsense->l3().press(); delay(200);
            dualsense->l3().release();
            break;
        case 7:
            Serial.println("R3");
            dualsense->r3().press(); delay(200);
            dualsense->r3().release();
            break;
        case 8:
            Serial.println("Select");
            dualsense->select().press(); delay(200);
            dualsense->select().release();
            break;
        case 9:
            Serial.println("Start");
            dualsense->start().press(); delay(200);
            dualsense->start().release();
            break;
        case 10:
            Serial.println("Home");
            dualsense->home().press(); delay(200);
            dualsense->home().release();
            break;
        case 11:
            Serial.println("Mute");
            dualsense->mute().press(); delay(200);
            dualsense->mute().release();
            break;
        case 12:
            Serial.println("DPad North");
            dualsense->dpad().setDirection(DPadDirection::N); delay(200);
            dualsense->dpad().release();
            break;
        case 13:
            Serial.println("DPad East");
            dualsense->dpad().setDirection(DPadDirection::E); delay(200);
            dualsense->dpad().release();
            break;
        case 14:
            Serial.println("DPad West");
            dualsense->dpad().setDirection(DPadDirection::W); delay(200);
            dualsense->dpad().release();
            break;
        case 15:
            Serial.println("DPad South");
            dualsense->dpad().setDirection(DPadDirection::S); delay(200);
            dualsense->dpad().release();
            break;
        case 16:
            Serial.println("L2 (analog + digital)");
            dualsense->leftTrigger().setValue(100); delay(200);
            dualsense->leftTrigger().setValue(0);   delay(200);
            dualsense->leftTrigger().press();       delay(200);
            dualsense->leftTrigger().release();
            break;
        case 17:
            Serial.println("R2 (analog + digital)");
            dualsense->rightTrigger().setValue(100); delay(200);
            dualsense->rightTrigger().setValue(0);   delay(200);
            dualsense->rightTrigger().press();       delay(200);
            dualsense->rightTrigger().release();
            break;
        case 18:
            Serial.println("Left stick left");
            dualsense->leftStick().set(-120, 0); delay(200);
            dualsense->leftStick().center();
            break;
        case 19:
            Serial.println("Left stick right");
            dualsense->leftStick().set(120, 0); delay(200);
            dualsense->leftStick().center();
            break;
        case 20:
            Serial.println("Left stick down");
            dualsense->leftStick().set(0, 120); delay(200);
            dualsense->leftStick().center();
            break;
        case 21:
            Serial.println("Left stick up");
            dualsense->leftStick().set(0, -120); delay(200);
            dualsense->leftStick().center();
            break;
        case 22:
            Serial.println("Right stick left");
            dualsense->rightStick().set(-120, 0); delay(200);
            dualsense->rightStick().center();
            break;
        case 23:
            Serial.println("Right stick right");
            dualsense->rightStick().set(120, 0); delay(200);
            dualsense->rightStick().center();
            break;
        case 24:
            Serial.println("Right stick down");
            dualsense->rightStick().set(0, 120); delay(200);
            dualsense->rightStick().center();
            break;
        case 25:
            Serial.println("Right stick up");
            dualsense->rightStick().set(0, -120); delay(200);
            dualsense->rightStick().center();
            break;
        case 26:
            Serial.println("Circle both sticks");
            for (float i = 0; i < TWO_PI; i += STEP) {
                dualsense->leftStick().set((int16_t)(cos(i)*100), (int16_t)(sin(i)*100));
                dualsense->rightStick().set((int16_t)(cos(i)*100), (int16_t)(-sin(i)*100));
                delay(15);
            }
            dualsense->leftStick().center();
            dualsense->rightStick().center();
            break;
        case 27:
            Serial.println("L4");
            dualsense->l4().press(); delay(200);
            dualsense->l4().release();
            break;
        case 28:
            Serial.println("R4");
            dualsense->r4().press(); delay(200);
            dualsense->r4().release();
            break;
        case 29:
            Serial.println("L5");
            dualsense->l5().press(); delay(200);
            dualsense->l5().release();
            break;
        case 30:
            Serial.println("R5");
            dualsense->r5().press(); delay(200);
            dualsense->r5().release();
            break;
        case 31:
            Serial.println("Gyro/Accel movement");
            for (float i = 0; i < TWO_PI; i += STEP) {
                dualsense->setAccel((int16_t)(cos(i)*300), (int16_t)(sin(i)*300), (int16_t)(tan(i)*300));
                dualsense->setGyro((int16_t)(cos(i)*400), (int16_t)(sin(i)*400), (int16_t)(tan(i)*400));
                delay(5);
            }
            break;
        case 32: {
            Serial.println("Touchpad movement + click");
            float i = 0;
            int8_t slot = dualsense->touchpadStartTouch((uint16_t)(cos(i)*400 + 1000), (uint16_t)(sin(i)*400 + 540));
            for (i = STEP; i < TWO_PI; i += STEP) {
                dualsense->touchpadUpdatePosition((uint16_t)(cos(i)*400 + 1000), (uint16_t)(sin(i)*400 + 540), slot);
                delay(15);
            }
            delay(20);
            Serial.println("Touchpad click");
            dualsense->touchpadButton().press(); delay(200);
            dualsense->touchpadButton().release();
            dualsense->touchpadStopTouch(slot);
            break;
        }
        case 33: {
            // Battery level test
            auto dwell = [&](uint32_t ms) {
                uint32_t end = millis() + ms;
                while (millis() < end) { dualsense->timestamp(); dualsense->seq(); delay(20); }
            };
            dualsense->battery().setCharging(false);
            for (int i = 100; i >= 0; i -= 25) {
                dualsense->battery().setLevel(i);
                compositeHID.setBatteryLevel(i);
                dwell(1000);
            }
            dualsense->battery().setCharging(true);
            for (int i = 0; i <= 100; i += 25) {
                dualsense->battery().setLevel(i);
                compositeHID.setBatteryLevel(i);
                dwell(1000);
            }
            break;
        }
        case 34:
            Serial.println("Trigger feedback sweep");
            for (int pos = 0; pos <= 255; pos += 5) {
                dualsense->setL2TriggerFeedback(0x11, pos, 0);
                dualsense->setR2TriggerFeedback(0x11, 255 - pos, 0);
                delay(15);
            }
            dualsense->setL2TriggerFeedback(0x00, 0, 0);
            dualsense->setR2TriggerFeedback(0x00, 0, 0);
            break;
        case 35:
            Serial.println("Status2 flags: headphones, mic, mute, USB");
            dualsense->setHeadphonesPlugged(true); delay(500);
            dualsense->setHeadphoneMic(true);      delay(500);
            dualsense->setMuteActive(true);        delay(500);
            dualsense->setUsbPlugged(true);        delay(1000);
            dualsense->setHeadphonesPlugged(false);
            dualsense->setHeadphoneMic(false);
            dualsense->setMuteActive(false);
            dualsense->setUsbPlugged(false);
            break;
        default:
            Serial.println("Invalid selection: " + String(selection));
            break;
    }
}
