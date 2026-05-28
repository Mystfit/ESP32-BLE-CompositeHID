#include <BleGamepad.h>
#include <KeyboardDevice.h>

XboxGamepadDevice* gamepad;
KeyboardDevice* keyboard;
BleCompositeHID compositeHID("ESP32 SeriesX Controller", "Mystfit", 100);

void setup()
{
    Serial.begin(115200);

    auto* config = new XboxSeriesXControllerDeviceConfiguration();
    gamepad = new XboxGamepadDevice(config);
    compositeHID.addDevice(gamepad);

    KeyboardConfiguration keyboardConfig;
    keyboardConfig.setAutoReport(false);
    keyboard = new KeyboardDevice(keyboardConfig);
    compositeHID.addDevice(keyboard);

    compositeHID.begin(config->getIdealHostConfiguration());
}

void loop()
{
    if (compositeHID.isConnected()) {
        gamepad->a().press();
        gamepad->sendReport();
        delay(500);
        gamepad->a().release();
        gamepad->sendReport();
        delay(100);

        keyboard->keyPress(KEY_A);
        keyboard->sendKeyReport();
        delay(500);
        keyboard->keyRelease(KEY_A);
        keyboard->sendKeyReport();
        delay(100);
    }
}
