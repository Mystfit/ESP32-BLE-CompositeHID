#include <Arduino.h>
#include <KeyboardDevice.h>
#include <BleCompositeHID.h>

#define KEY_RANGE_MIN KEY_A
#define KEY_RANGE_MAX KEY_RIGHTMETA

#define MODKEY_RANGE_MIN KEY_MOD_LALT
#define MODKEY_RANGE_MAX KEY_MOD_RMETA

BleCompositeHID compositeHID("ESP32 Keyboard", "Mystfit", 100);
KeyboardDevice keyboard;

void setup()
{
    Serial.begin(115200);

    // Set our advertised appearance to keyboard
    bleHostConfig.setHidType(HID_KEYBOARD);

    // Set our advertised appearance to keyboard
    bleHostConfig.setHidType(HID_KEYBOARD);

    compositeHID.addDevice(keyboard);
    compositeHID.begin(bleHostConfig);

    Serial.println("Waiting for connection");
    delay(3000);
}

void loop()
{
    if (compositeHID.isConnected())
    {
        uint8_t modKey = 0x01;
        for(uint8_t modKeyCount = 0; modKeyCount < 8; modKeyCount++)
        {
            Serial.println("Pressing modifier key " + String(modKey));
            keyboard->modifierKeyPress(modKey);
            delay(10);
            keyboard->modifierKeyRelease(modKey);
            delay(1000);
            modKey = modKey << 1;
        }

        for(uint8_t keyCode = KEY_RANGE_MIN; keyCode <= KEY_RANGE_MAX; keyCode++)
        {
            Serial.println("Pressing key " + String(keyCode));
            keyboard.keyPress(keyCode);
            delay(10);
            keyboard.keyRelease(keyCode);
            delay(1000);
        }
    }
}
