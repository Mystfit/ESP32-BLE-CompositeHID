#include "SteamController2026GamepadConfiguration.h"
#include "SteamController2026GamepadDevice.h"
#include "NimBLEHIDDevice.h"
#include <string.h>

SteamController2026DeviceConfiguration::SteamController2026DeviceConfiguration(uint8_t reportId)
    : BaseCompositeDeviceConfiguration(reportId)
{
}

BLEHostConfiguration SteamController2026DeviceConfiguration::getIdealHostConfiguration() const
{
    BLEHostConfiguration config;
    config.setHidType(HID_GAMEPAD);
    config.setVidSource(VENDOR_USB_SOURCE);
    config.setVid(SC2026_VENDOR_ID);
    config.setPid(SC2026_PRODUCT_ID);
    config.setGuidVersion(SC2026_BCD_DEVICE);
    config.setSerialNumber(SC2026_SERIAL);
    config.setQueueSendRate(0);
    return config;
}

uint8_t SteamController2026DeviceConfiguration::getDeviceReportSize() const
{
    return sizeof(SteamController2026InputReport);
}

size_t SteamController2026DeviceConfiguration::makeDeviceReport(uint8_t* buffer, size_t bufferSize) const
{
    size_t descSize = sizeof(SC2026_HIDDescriptor);
    if (descSize > bufferSize)
        return (size_t)-1;
    memcpy(buffer, SC2026_HIDDescriptor, descSize);
    return descSize;
}
