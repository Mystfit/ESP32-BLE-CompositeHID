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
 * of the session. This appears to be a Steam-client-side state machine that only unlocks
 * haptics after the gyro-calibration UI has been viewed; it is not a limitation of this
 * firmware. Tested workarounds that did NOT help: matching feature-report sizes to the HID
 * descriptor, proactively indicating calibration on input subscribe, forcing GATT Service
 * Changed to re-read calibration, and spoofing a Sony Bluetooth OUI.
 *
 * Advanced haptic features (audio-based vibration, adaptive triggers) still require a USB
 * connection — over Bluetooth only basic dual-motor rumble is supported, matching the real
 * DualSense.
 *
 * BLE-REACHABLE HAPTIC PROXY:
 * True VCA haptic audio (what DSX calls "BT Haptics" / "Sound Waves") is streamed over the
 * Bluetooth Classic HID interrupt channel via an undocumented Sony firmware path. It does
 * not reach BLE HoGP peripherals. If you want a host-driven haptic signal that DOES survive
 * BLE, the adaptive-trigger "Vibration" effect (DS_TRIGGER_EFFECT_VIBRATION, type 0x26)
 * travels inside the standard 0x31 output report and carries frequency + amplitude. You can
 * drive an external LRA/VCA amplifier from those values — see OnVibrateEvent below.
 */

#include <BleConnectionStatus.h>
#include <BleCompositeHID.h>
#include <DualsenseGamepadDevice.h>
#include "ArduinoDefines.h"
#define CONFIG_BT_NIMBLE_EXT_ADV 1

int ledPin = LED_BUILTIN; // LED connected to digital pin 8
uint8_t LEDmode = 0;
uint8_t playerLEDs = 0x00;
uint8_t ledcolor[3] = {0,0,0};
uint8_t motor_left = 0;
uint8_t motor_right = 0;
uint8_t muteled = 0;
DualsenseGamepadDevice* dualsense;
BleCompositeHID compositeHID("Libresteishon Edge", "YeaSeb", 100);

// FunctionSlots must be global to persist after setup() completes
FunctionSlot<DualsenseGamepadOutputReportData> vibrationSlot(OnVibrateEvent);
FunctionSlot<DualsenseGamepadOutputReportData> ledSlot(OnLEDEvent);

void OnLEDEvent(DualsenseGamepadOutputReportData data)
{
    if(data.lightbar_setup != LEDmode){
        LEDmode = data.lightbar_setup;
        Serial.println(String("LED mode changed:") + data.lightbar_setup);
    }

    if (data.player_leds != playerLEDs) {
        playerLEDs=data.player_leds;
        if ((data.player_leds & DUALSENSE_PLAYERLED_ON) == DUALSENSE_PLAYERLED_ON) {
            if ((data.player_leds & DUALSENSE_PLAYERLED_1) == DUALSENSE_PLAYERLED_1) {
                Serial.println("led 1 on");
            } else {
                Serial.println("led 1 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_2) == DUALSENSE_PLAYERLED_2) {
                Serial.println("led 2 on");
            } else {
                Serial.println("led 2 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_3) == DUALSENSE_PLAYERLED_3) {
                Serial.println("led 3 on");
            } else {
                Serial.println("led 3 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_4) == DUALSENSE_PLAYERLED_4) {
                Serial.println("led 4 on");
            } else {
                Serial.println("led 4 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_5) == DUALSENSE_PLAYERLED_5) {
                Serial.println("led 5 on");
            } else {
                Serial.println("led 5 off");
            }
        }
    } 

    if (data.lightbar_red != ledcolor[0] || data.lightbar_green != ledcolor[1] || data.lightbar_blue != ledcolor[2]){
        ledcolor[0]=data.lightbar_red;
        ledcolor[1]=data.lightbar_green;
        ledcolor[2]=data.lightbar_blue;
        Serial.println(String("LED color change, R: ") + ledcolor[0] + " G: " + ledcolor[1] + " B: " + ledcolor[2]);
    }

    if (data.mute_button_led != muteled)
    {
        muteled = data.mute_button_led;
        Serial.println(String("Mute button change: ")+muteled);
    }
}

String formatTriggerEffect(const DualsenseGamepadOutputReportData::ParsedTriggerEffect& t)
{
    String s;
    switch (t.subtype()) {
        case DS_TRIGGER_SUBTYPE_OFF:
            s = "off";
            break;

        case DS_TRIGGER_SUBTYPE_FEEDBACK: {
            auto fb = t.asFeedback();
            s = "feedback(start=" + String(fb.start_position)
              + ",strength=" + String(fb.strength())
              + ",mask=0x" + String(fb.position_mask, HEX) + ")";
            break;
        }

        case DS_TRIGGER_SUBTYPE_SLOPE_FEEDBACK: {
            auto sl = t.asSlope();
            s = "slope(start=" + String(sl.start_position)
              + ",end=" + String(sl.end_position)
              + ",startStr=" + String(sl.start_strength)
              + ",endStr=" + String(sl.end_strength)
              + ",mask=0x" + String(sl.position_mask, HEX) + ")";
            break;
        }

        case DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_FEEDBACK: {
            auto mp = t.asMultiPosition();
            s = "multipos(mask=0x" + String(mp.position_mask, HEX) + ",strengths=[";
            for (int i = 0; i < 10; i++) {
                if (i) s += ",";
                s += String(mp.per_position_strength[i]);
            }
            s += "])";
            break;
        }

        case DS_TRIGGER_SUBTYPE_WEAPON: {
            auto w = t.asWeapon();
            s = "weapon(start=" + String(w.start_position)
              + ",end=" + String(w.end_position)
              + ",strength=" + String(w.strength)
              + ",mask=0x" + String(w.position_mask, HEX) + ")";
            break;
        }

        case DS_TRIGGER_SUBTYPE_VIBRATION: {
            auto v = t.asVibration();
            s = "vibration(start=" + String(v.start_position)
              + ",amp=" + String(v.amplitude())
              + ",freq=" + String(v.frequency) + "Hz"
              + ",mask=0x" + String(v.position_mask, HEX) + ")";
            break;
        }

        case DS_TRIGGER_SUBTYPE_MULTIPLE_POSITION_VIBRATION: {
            auto mv = t.asMultiVibration();
            s = "multivib(freq=" + String(mv.frequency) + "Hz,amps=[";
            for (int i = 0; i < 10; i++) {
                if (i) s += ",";
                s += String(mv.per_position_amplitude[i]);
            }
            s += "])";
            break;
        }

        default:
            s = "unknown(0x" + String(t.mode, HEX) + ",params=[";
            for (int i = 0; i < 10; i++) {
                if (i) s += ",";
                s += String(t.raw_params[i]);
            }
            s += "])";
            break;
    }
    return s;
}

void OnVibrateEvent(DualsenseGamepadOutputReportData data)
{
    String message = "";

    // Classic rumble. weakMotor()/strongMotor() are aliases for
    // motor_left/motor_right that name the canonical wire roles.
    if (data.hasRumble()) {
        if(data.weakMotor() != motor_left){
            motor_left = data.weakMotor();
            message += "rumble_weak_motor:" + String(motor_left);
        }
        if(data.strongMotor() != motor_right){
            motor_right = data.strongMotor();
            if(!message.isEmpty()) message += ",";
            message += "rumble_strong_motor:" + String(motor_right);
        }
    }

    // Log all adaptive-trigger effects whenever the host sets the effect flags.
    auto lt = data.leftTrigger();
    auto rt = data.rightTrigger();
    if (data.hasLeftTriggerEffect()) {
        if(!message.isEmpty()) message += ",";
        message += "L2=" + formatTriggerEffect(lt);
    }
    if (data.hasRightTriggerEffect()) {
        if(!message.isEmpty()) message += ",";
        message += " R2=" + formatTriggerEffect(rt);
    }

    // Raw byte dump for active trigger effects — helps verify decoded values.
    // Shows mode + p[0..9]; for Vibration: freq is at p[9] (single) or p[8] (multi).
    // if (data.hasLeftTriggerEffect()) {
    //     String d = " L2 raw: " + String(lt.mode);
    //     for (int i = 0; i < 10; ++i) d += "," + String(lt.raw_params[i]);
    //     message += d;
    // }
    // if (data.hasRightTriggerEffect()) {
    //     String d = " R2 raw: " + String(rt.mode);
    //     for (int i = 0; i < 10; ++i) d += "," + String(rt.raw_params[i]);
    //     message += d;
    // }

    if(!message.isEmpty()) Serial.println(message);

    if (motor_left > 0 || motor_right > 0) {
        digitalWrite(ledPin, HIGH);
    } else {
        digitalWrite(ledPin, LOW);
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT); // sets the digital pin as output

    DualsenseEdgeControllerDeviceConfiguration* config = new DualsenseEdgeControllerDeviceConfiguration();
    config->setAutoReport(true);
    config->setAutoDefer(false);

    // The composite HID device pretends to be a valid Dualsense controller via vendor and product IDs (VID/PID).
    // Platforms like windows/linux need this
    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
    Serial.println("Using VID source: " + String(hostConfig.getVidSource(), HEX));
    Serial.println("Using VID: " + String(hostConfig.getVid(), HEX));
    Serial.println("Using PID: " + String(hostConfig.getPid(), HEX));
    Serial.println("Using GUID version: " + String(hostConfig.getGuidVersion(), HEX));
    Serial.println("Using serial number: " + String(hostConfig.getSerialNumber()));

    // Set up dualsense
    dualsense = new DualsenseGamepadDevice(config);

    // Attach event handlers (FunctionSlots are defined globally)
    dualsense->onReceivedOutputReport.attach(vibrationSlot);
    dualsense->onReceivedOutputReport.attach(ledSlot);

    // Add all child devices to the top-level composite HID device to manage them
    compositeHID.addDevice(dualsense);

    // Start the composite HID device to broadcast HID reports
    // Serial.println("Starting composite HID device...");
    compositeHID.begin(hostConfig);

    while(!compositeHID.isConnected()){
        delay(100);
    }
    delay(280);

    dualsense->sendPairingInfoReport();
    dualsense->sendFirmInfoReport();
    dualsense->sendCalibrationReport();
    dualsense->resetInputs();

    Serial.println("Select button/axis to be activated for testing (3 digits in the format 000). 0: Cross 1: Circle  2: Square  3: Triangle \n 4: L1 5: R1 6: L3 7: R3 8: select 9: start 10: Home 11:VolMute \n 12: DPAD u 13: DPAD R 14: DPAD L 15: DPAD D \n 16: L2 17: R2 \n 18:left stick left 19:left stick right 20:left stick down 21:left stick up \n 22:right stick left 23:right stick right 24:right stick down 25:right stick up \n 26: circle joysticks 27: L4 28 : R4 29: L5 30: R5 31: Gyro/Accel 32: Touchpad");
}

void loop()
{
    if (compositeHID.isConnected()) {
        int selection = -1;
        const float STEP = 0.05; // angle change per frame (speed)

        dualsense->timestamp();
        dualsense->seq();

        if(Serial.available() < 2) {
            dualsense->sendGamepadReport();
            delay(20);
            yield();
            return;
        }
       
        selection = Serial.parseInt();
        Serial.println(selection);

        switch (selection) {
            case 0: {
                Serial.println("Pressing Cross");
                dualsense->press(DUALSENSE_BUTTON_A);
                delay(500);
                dualsense->release(DUALSENSE_BUTTON_A);
                break;
            }
            case 1: {
                dualsense->press(DUALSENSE_BUTTON_B);
                Serial.println("Pressing Circle");
                delay(100);
                dualsense->release(DUALSENSE_BUTTON_B);
                break;
            }
            case 2: {
                dualsense->press(DUALSENSE_BUTTON_X);
                Serial.println("Pressing Square");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_X);
                break;
            }
            case 3:
                dualsense->press(DUALSENSE_BUTTON_Y);
                Serial.println("Pressing Triangle");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_Y);
                break;
            case 4:
                dualsense->press(DUALSENSE_BUTTON_LB);
                Serial.println("Pressing L1");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_LB);
                break;
            case 5:
                dualsense->press(DUALSENSE_BUTTON_RB);
                Serial.println("Pressing R1");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_RB);
                break;
            case 6:
                dualsense->press(DUALSENSE_BUTTON_LS);
                Serial.println("Pressing L3");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_LS);
                break;
            case 7:
                dualsense->press(DUALSENSE_BUTTON_RS);
                Serial.println("Pressing R3");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_RS);
                break;
            case 8:
                dualsense->press(DUALSENSE_BUTTON_SELECT);
                Serial.println("Pressing Select");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_SELECT);
                break;
            case 9:
                dualsense->press(DUALSENSE_BUTTON_START);
                Serial.println("Pressing Start");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_START);
                break;
            case 10:
                dualsense->press(DUALSENSE_BUTTON_MODE);
                Serial.println("Pressing Home");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_MODE);
                break;
            case 11:
                dualsense->press(DUALSENSE_BUTTON_MUTE);
                Serial.println("Pressing Mute");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_MUTE);
                break;
            case 12:
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_NORTH);
                Serial.println("Pressing DPAD UP");
                delay(200);
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_NONE);
                break;
            case 13:
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_EAST);
                Serial.println("Pressing DPAD RIGHT");
                delay(200);
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_NONE);
                break;
            case 14:
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_WEST);
                Serial.println("Pressing DPAD LEFT");
                delay(200);
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_NONE);
                break;
            case 15:
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_SOUTH);
                Serial.println("Pressing DPAD DOWN");
                delay(200);
                dualsense->pressDPadDirection(DUALSENSE_BUTTON_DPAD_NONE);
                break;
            case 16:
                dualsense->setLeftTrigger(100);
                Serial.println("Pressing L2");
                delay(200);
                dualsense->setLeftTrigger(0);
                delay(200);
                dualsense->press(DUALSENSE_BUTTON_LT);
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_LT);
                break;
            case 17:
                dualsense->setRightTrigger(100);
                Serial.println("Pressing R2");
                delay(200);
                dualsense->setRightTrigger(0);
                delay(200);
                dualsense->press(DUALSENSE_BUTTON_RT);
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_RT);
                break;
            case 18:
                dualsense->setLeftThumb(-120, 0);
                Serial.println("Moving left_analog left");
                delay(200);
                dualsense->setLeftThumb(0, 0);
                break;
            case 19:
                dualsense->setLeftThumb(120, 0);
                Serial.println("Moving left_analog Right");
                delay(200);
                dualsense->setLeftThumb(0, 0);
                break;
            case 20: {
                dualsense->setLeftThumb(0, 120);
                Serial.println("Moving left_analog down");
                delay(200);
                dualsense->setLeftThumb(0, 0);
                break;
            }
            case 21: {
                dualsense->setLeftThumb(0, -120);
                Serial.println("Moving left_analog up");
                delay(200);
                dualsense->setLeftThumb(0, 0);
                break;
            }
            case 22: {
                dualsense->setRightThumb(-120, 0);
                Serial.println("Moving right_analog left");
                delay(200);
                dualsense->setRightThumb(0, 0);
                break;
            }
            case 23: {
                dualsense->setRightThumb(120, 0);
                Serial.println("Moving right_analog Right");
                delay(200);
                dualsense->setRightThumb(0, 0);
                break;
            }
            case 24: {
                dualsense->setRightThumb(0, 120);
                Serial.println("Moving right_analog down");
                delay(200);
                dualsense->setRightThumb(0, 0);
                break;
            }
            case 25: {
                dualsense->setRightThumb(0, -120);
                Serial.println("Moving right_analog up");
                delay(200);
                dualsense->setRightThumb(0, 0);
                break;
            }
            case 26: {
                Serial.println("Circle both joysticks");
                for (float i = 0; i < TWO_PI; i += STEP) {
                    dualsense->setLeftThumb(cos(i) * 100, sin(i) * 100);
                    dualsense->setRightThumb(cos(i) * 100, -sin(i) * 100);
                    delay(15);
                }
                dualsense->setRightThumb(0, 0);
                dualsense->setLeftThumb(0, 0);
                break;
            }
            case 27: {
                dualsense->press(DUALSENSE_BUTTON_L4);
                Serial.println("Pressing L4");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_L4);
                break;
            }
            case 28: {
                dualsense->press(DUALSENSE_BUTTON_R4);
                Serial.println("Pressing R4");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_R4);
                break;
            }
            case 29: {
                dualsense->press(DUALSENSE_BUTTON_L5);
                Serial.println("Pressing L5");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_L5);
                break;
            }
            case 30: {
                dualsense->press(DUALSENSE_BUTTON_R5);
                Serial.println("Pressing R5");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_R5);
                break;
            }
            case 31: {
                Serial.println("sending gyro/accel movement");
                for (float i = 0; i < TWO_PI; i += STEP) {
                    dualsense->setAccel(cos(i) * 300, sin(i) * 300, tan(i) * 300);
                    dualsense->setGyro(cos(i) * 400, sin(i) * 400, tan(i) * 400);
                    delay(5);
                }
                break;
            }
            case 32: {
                Serial.println("Sending touchpad movement");
                delay(200);
                for (float i = 0; i < TWO_PI; i += STEP) {
                    dualsense->setLeftTouchpad(cos(i) * 400 + 1000, sin(i) * 400 + 540);
                    delay(15);
                }
                delay(20);
                Serial.println("Sending touchpad click");
                dualsense->press(DUALSENSE_BUTTON_TOUCHPAD);
                Serial.println("Pressing R5");
                delay(200);
                dualsense->release(DUALSENSE_BUTTON_TOUCHPAD);
                dualsense->releaseLeftTouchpad();
                break;
            }
            case 33: {
                // DualSense controllers can send battery info through the input report as well as through the BLE device.
                // Pump seq()/timestamp() during each dwell so reports don't carry stale sequence numbers that hosts
                // (DSX/Windows) may dedupe and drop.
                auto dwell = [&](uint32_t ms) {
                    uint32_t end = millis() + ms;
                    while (millis() < end) {
                        dualsense->timestamp();
                        dualsense->seq();
                        delay(20);
                    }
                };

                dualsense->setChargingStatus(false);
                for (int i = 100; i >= 0; i -= 25) {
                    dualsense->setBatteryLevel(i);
                    compositeHID.setBatteryLevel(i);
                    dwell(1000);
                }
                dualsense->setChargingStatus(true);
                for (int i = 0; i <= 100; i += 25) {
                    dualsense->setBatteryLevel(i);
                    compositeHID.setBatteryLevel(i);
                    dwell(1000);
                }
                break;
            }
            default: {
                Serial.println("Selection invalid");
                Serial.println(selection);
            }
        }
    } else {
        Serial.println("disconnected");
        delay(500);
    }
}
