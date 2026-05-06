#ifndef DUALSENSE_GAMEPAD_CONFIGURATION_H
#define DUALSENSE_GAMEPAD_CONFIGURATION_H

#include "BaseCompositeDevice.h"
#include "DualsenseDescriptors.h"

class DualsenseGamepadDeviceConfiguration : public BaseCompositeDeviceConfiguration {
public:
    DualsenseGamepadDeviceConfiguration(uint8_t reportId = DUALSENSE_EDGE_INPUT_REPORT_ID);
    virtual ~DualsenseGamepadDeviceConfiguration() = default;
    virtual uint8_t getDeviceReportSize() const override { return 0; }
    virtual size_t makeDeviceReport(uint8_t* buffer, size_t bufferSize) const override
    {
        return -1;
    }
};

class DualsenseEdgeControllerDeviceConfiguration : public DualsenseGamepadDeviceConfiguration {
public:
    virtual const char* getDeviceName() const { return "DualSense Edge"; }
    virtual BLEHostConfiguration getIdealHostConfiguration() const override;
    virtual uint8_t getDeviceReportSize() const override;
    virtual size_t makeDeviceReport(uint8_t* buffer, size_t bufferSize) const override;
};

#endif // DUALSENSE_GAMEPAD_CONFIGURATION_H
