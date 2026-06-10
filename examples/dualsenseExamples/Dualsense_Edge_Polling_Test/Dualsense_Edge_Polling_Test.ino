#include <BleGamepad.h>
#define CONFIG_BT_NIMBLE_EXT_ADV 1

int ledPin = 8;
DualsenseEdgeGamepadDevice* dualsense;
BleCompositeHID compositeHID("Libresteishon Edge", "YeaSeb", 100);

FunctionSlot<RumbleState> rumbleSlot(OnRumbleEvent);

void OnRumbleEvent(RumbleState state)
{
    digitalWrite(ledPin, (state.weak > 0 || state.strong > 0) ? LOW : HIGH);
}

void setup()
{
    pinMode(ledPin, OUTPUT);

    auto* config = new DualsenseEdgeControllerDeviceConfiguration();
    config->setAutoReport(false);
    config->setAutoDefer(false);
    dualsense = new DualsenseEdgeGamepadDevice(config);
    compositeHID.addDevice(dualsense);

    dualsense->rumble().onRumble.attach(rumbleSlot);

    compositeHID.begin(config->getIdealHostConfiguration());
}

void loop()
{
    if (!compositeHID.isConnected()) {
        delay(500);
        return;
    }

    delay(150);
    dualsense->sendPairingInfoReport();
    dualsense->sendFirmInfoReport();
    dualsense->sendCalibrationReport();
    delay(4000);
    dualsense->resetInputs();

    const float STEP = 0.05f;
    while (compositeHID.isConnected()) {
        for (float i = 0; i < TWO_PI * 500; i += STEP) {
            dualsense->timestamp();
            dualsense->leftStick().set((int16_t)(cos(i) * 100), (int16_t)(sin(i) * 100));
            dualsense->sendReport();
            delayMicroseconds(900);
            yield();
        }
        dualsense->leftStick().center();
    }
}
