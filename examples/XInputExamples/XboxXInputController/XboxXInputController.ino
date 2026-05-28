#include <BleGamepad.h>

int ledPin = 5;

XboxGamepadDevice* gamepad;
BleCompositeHID compositeHID("ESP32 SeriesX Controller", "Mystfit", 100);

FunctionSlot<RumbleState> vibrationSlot(OnRumbleEvent);

void OnRumbleEvent(RumbleState state)
{
    digitalWrite(ledPin, (state.weak > 0 || state.strong > 0) ? LOW : HIGH);
    Serial.println("Rumble — weak: " + String(state.weak) + " strong: " + String(state.strong));
}

void setup()
{
    Serial.begin(115200);
    pinMode(ledPin, OUTPUT);

    // Switch to XboxOneSControllerDeviceConfiguration for older compatibility.
    auto* config = new XboxSeriesXControllerDeviceConfiguration();
    gamepad = new XboxGamepadDevice(config);
    compositeHID.addDevice(gamepad);

    gamepad->rumble().onRumble.attach(vibrationSlot);

    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
    Serial.println("VID: 0x" + String(hostConfig.getVid(), HEX)
                 + "  PID: 0x" + String(hostConfig.getPid(), HEX));
    compositeHID.begin(hostConfig);
}

void loop()
{
    if (compositeHID.isConnected()) {
        testButtons();
        testPads();
        testTriggers();
        testThumbsticks();
    }
}

void testButtons()
{
    void (*presses[])() = {
        []{ gamepad->a().press(); }, []{ gamepad->b().press(); },
        []{ gamepad->x().press(); }, []{ gamepad->y().press(); },
        []{ gamepad->lb().press(); }, []{ gamepad->rb().press(); },
        []{ gamepad->start().press(); }, []{ gamepad->select().press(); },
        []{ gamepad->ls().press(); }, []{ gamepad->rs().press(); }
    };
    void (*releases[])() = {
        []{ gamepad->a().release(); }, []{ gamepad->b().release(); },
        []{ gamepad->x().release(); }, []{ gamepad->y().release(); },
        []{ gamepad->lb().release(); }, []{ gamepad->rb().release(); },
        []{ gamepad->start().release(); }, []{ gamepad->select().release(); },
        []{ gamepad->ls().release(); }, []{ gamepad->rs().release(); }
    };
    for (int i = 0; i < 10; i++) {
        presses[i]();  gamepad->sendReport(); delay(500);
        releases[i](); gamepad->sendReport(); delay(100);
    }
    gamepad->share().press();  gamepad->sendReport(); delay(500);
    gamepad->share().release(); gamepad->sendReport(); delay(100);
}

void testPads()
{
    const DPadDirection dirs[] = {
        DPadDirection::N, DPadDirection::NE, DPadDirection::E, DPadDirection::SE,
        DPadDirection::S, DPadDirection::SW, DPadDirection::W, DPadDirection::NW
    };
    for (DPadDirection d : dirs) {
        gamepad->dpad().setDirection(d); gamepad->sendReport(); delay(500);
        gamepad->dpad().release();       gamepad->sendReport(); delay(100);
    }
}

void testTriggers()
{
    for (int v = XBOX_TRIGGER_MIN; v <= XBOX_TRIGGER_MAX; v++) {
        gamepad->leftTrigger().setValue(v);
        gamepad->rightTrigger().setValue(v);
        gamepad->sendReport();
        delay(8);
    }
}

void testThumbsticks()
{
    int startTime = millis();
    while (millis() - startTime < 8000) {
        int16_t x = (int16_t)(cos((float)millis() / 1000.0f) * XBOX_STICK_MAX);
        int16_t y = (int16_t)(sin((float)millis() / 1000.0f) * XBOX_STICK_MAX);
        gamepad->leftStick().set(x, y);
        gamepad->rightStick().set(x, y);
        gamepad->sendReport();
        delay(8);
    }
}
