#ifndef STEAM_CONTROLLER_2026_GAMEPAD_CONFIGURATION_H
#define STEAM_CONTROLLER_2026_GAMEPAD_CONFIGURATION_H

#include "BaseCompositeDevice.h"
#include "SteamController2026Descriptors.h"

class SteamController2026DeviceConfiguration : public BaseCompositeDeviceConfiguration {
public:
    SteamController2026DeviceConfiguration(uint8_t reportId = SC2026_INPUT_REPORT_ID);
    virtual ~SteamController2026DeviceConfiguration() = default;
    virtual const char* getDeviceName() const override { return "Steam Controller"; }
    virtual BLEHostConfiguration getIdealHostConfiguration() const override;
    virtual uint8_t getDeviceReportSize() const override;
    virtual size_t makeDeviceReport(uint8_t* buffer, size_t bufferSize) const override;
};

#endif // STEAM_CONTROLLER_2026_GAMEPAD_CONFIGURATION_H
