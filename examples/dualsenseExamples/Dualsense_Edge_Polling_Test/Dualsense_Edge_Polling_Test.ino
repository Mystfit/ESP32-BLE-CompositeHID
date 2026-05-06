#include <BleConnectionStatus.h>

#include <BleCompositeHID.h>
#include <DualsenseGamepadDevice.h>
//The nimBLE documentation mentions this helping with achieving higher speeds by advertising certain BLE5 features
#define CONFIG_BT_NIMBLE_EXT_ADV 1

int ledPin = 8; // LED connected to digital pin 13
uint8_t playerLEDs = 0x00;
uint8_t ledcolor[3] = {0,0,0}; 
uint8_t muteled = 0;
DualsenseGamepadDevice* dualsense;
BleCompositeHID compositeHID("Libresteishon Edge", "YeaSeb", 100);
void onLEDsetup(DualsenseGamepadOutputReportData data)
{
    if (data.player_leds != playerLEDs) {
        playerLEDs=data.player_leds;
        if ((data.player_leds & DUALSENSE_PLAYERLED_ON) == DUALSENSE_PLAYERLED_ON) {
            if ((data.player_leds & DUALSENSE_PLAYERLED_1) == DUALSENSE_PLAYERLED_1) {
                //Serial.println("led 1 on");
            } else {
                //Serial.println("led 1 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_2) == DUALSENSE_PLAYERLED_2) {
                //Serial.println("led 2 on");
            } else {
                //Serial.println("led 2 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_3) == DUALSENSE_PLAYERLED_3) {
                //Serial.println("led 3 on");
            } else {
                //Serial.println("led 3 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_4) == DUALSENSE_PLAYERLED_4) {
                //Serial.println("led 4 on");
            } else {
                //Serial.println("led 4 off");
            }
            if ((data.player_leds & DUALSENSE_PLAYERLED_5) == DUALSENSE_PLAYERLED_5) {
                //Serial.println("led 5 on");
            } else {
                //Serial.println("led 5 off");
            }
        }
    }
    if (data.lightbar_red!=ledcolor[0] || data.lightbar_green!=ledcolor[1] || data.lightbar_blue!=ledcolor[2]){
        ledcolor[0]=data.lightbar_red;
        ledcolor[1]=data.lightbar_green;
        ledcolor[2]=data.lightbar_blue;
        //Serial.println(String("LED color change, R: ") + ledcolor[0] + " G: " + ledcolor[1] + " B: " + ledcolor[2]);
    }
    if (data.mute_button_led!=muteled)
    {
        muteled = data.mute_button_led;
        //Serial.println(String("Mute button change: ")+muteled);
    }
}
void OnVibrateEvent(DualsenseGamepadOutputReportData data)
{

    if (data.motor_right > 0 || data.motor_left > 0) {
        digitalWrite(ledPin, LOW);
    } else {
        digitalWrite(ledPin, HIGH);
    }
    if (data.lightbar_setup>0 || data.lightbar_red>0 || data.lightbar_green>0 || data.lightbar_blue>0 || data.mute_button_led>0 || data.player_leds>0){
        onLEDsetup(data);
    }
    //Serial.println("Output event. Weak motor: " + String(data.motor_left) + " Strong motor: " + String(data.motor_right) + " LED change:" + data.lightbar_setup);

}

void setup()
{
    //Serial.begin(115200);
    pinMode(ledPin, OUTPUT); // sets the digital pin as output

    DualsenseEdgeControllerDeviceConfiguration* config = new DualsenseEdgeControllerDeviceConfiguration();
    config->setAutoReport(false);
    config->setAutoDefer(false);

    // The composite HID device pretends to be a valid Dualsense controller via vendor and product IDs (VID/PID).
    // Platforms like windows/linux need this
    BLEHostConfiguration hostConfig = config->getIdealHostConfiguration();
    //Serial.println("Using VID source: " + String(hostConfig.getVidSource(), HEX));
    //Serial.println("Using VID: " + String(hostConfig.getVid(), HEX));
    //Serial.println("Using PID: " + String(hostConfig.getPid(), HEX));
    //Serial.println("Using GUID version: " + String(hostConfig.getGuidVersion(), HEX));
    //Serial.println("Using serial number: " + String(hostConfig.getSerialNumber()));

    // Set up dualsense
    dualsense = new DualsenseGamepadDevice(config);
    // Set up vibration event handler
    FunctionSlot<DualsenseGamepadOutputReportData> vibrationSlot(OnVibrateEvent);
    dualsense->onReceivedOutputReport.attach(vibrationSlot);

    // Add all child devices to the top-level composite HID device to manage them
    compositeHID.addDevice(dualsense);
    // Start the composite HID device to broadcast HID reports
    // Serial.println("Starting composite HID device...");
    compositeHID.begin(hostConfig);
}

void loop()
{
    if (compositeHID.isConnected()) {
        //Sometimes the driver doesn't parse the descriptor correctly without this delay
        delay(150);
        dualsense->sendPairingInfoReport();

        dualsense->sendFirmInfoReport();

        dualsense->sendCalibrationReport();
        //this one also helps, but it doesn't need to be this high for most hosts
        delay(4000);
        dualsense->resetInputs();
        int selection = 32;
        const float STEP = 0.05; // angle change per frame (speed)
        //for use with polling rate measuring tools like gamepadla
        while (compositeHID.isConnected()) {
                for (float i = 0; i < TWO_PI*500; i += STEP) {
                    dualsense->timestamp();
                    dualsense->setLeftThumb(cos(i) * 100, sin(i) * 100);
                    //dualsense->setRightThumb(cos(i) * 100, -sin(i) * 100);
                    dualsense->sendGamepadReport();
                    delayMicroseconds(900);
                    yield();
                }
                //dualsense->setRightThumb(0, 0);
                dualsense->setLeftThumb(0, 0);
        }
            
    } else {
        //Serial.println("disconnected");
        delay(500);
    }
}
