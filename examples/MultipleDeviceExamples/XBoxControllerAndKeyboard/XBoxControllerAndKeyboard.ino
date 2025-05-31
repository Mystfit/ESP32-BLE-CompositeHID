#include <BleConnectionStatus.h>

#include <BleCompositeHID.h>
#include <XboxGamepadDevice.h>
#include <KeyboardDevice.h>


XboxGamepadDevice *gamepad;
KeyboardDevice *keyboard;

BleCompositeHID compositeHID("ESP32 SeriesX Controller", "Mystfit", 100);

void setup()
{
    Serial.begin(115200);

    // Uncomment one of the following two config types depending on which controller version you want to use
    // The XBox series X controller only works on linux kernels >= 6.5
    
    //XboxOneSControllerDeviceConfiguration* config = new XboxOneSControllerDeviceConfiguration();
    XboxSeriesXControllerDeviceConfiguration* config = new XboxSeriesXControllerDeviceConfiguration();

    // The composite HID device pretends to be a valid Xbox controller via vendor and product IDs (VID/PID).
    // Platforms like windows/linux need this in order to pick an XInput driver over the generic BLE GATT HID driver. 
    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
    
    // Set up gamepad
    gamepad = new XboxGamepadDevice(config);

    // Set up keyboard
    KeyboardConfiguration keyboardConfig;
    keyboardConfig.setAutoReport(false);
    keyboard = new KeyboardDevice(keyboardConfig);

    // Add the gamepad child devices to the composite HID device first so it will appear in the HID report descriptor first for XInput
    compositeHID.addDevice(gamepad);
    compositeHID.addDevice(keyboard);

    // Start the composite HID device to broadcast HID reports
    Serial.println("Starting composite HID device...");
    compositeHID.begin(hostConfig);
}

void loop()
{
    if(compositeHID.isConnected()){
        // Test gamepad button
        gamepad->press(XBOX_BUTTON_A);
        gamepad->sendGamepadReport();
        delay(500);
        gamepad->release(XBOX_BUTTON_A);
        gamepad->sendGamepadReport();
        delay(100);

        // Test keyboard
        keyboard->keyPress(KEY_A);
        keyboard->sendKeyReport();
        delay(500);
        keyboard->keyRelease(KEY_A);
        keyboard->sendKeyReport();
        delay(100);
    }
}
