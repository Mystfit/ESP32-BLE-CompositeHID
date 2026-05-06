#include "DualsenseGamepadConfiguration.h"
#include "DualsenseGamepadDevice.h"
#include "NimBLEHIDDevice.h"

// DualsenseGamepadDeviceConfiguration methods
DualsenseGamepadDeviceConfiguration::DualsenseGamepadDeviceConfiguration(uint8_t reportId)
    : BaseCompositeDeviceConfiguration(reportId)
{
}

BLEHostConfiguration DualsenseEdgeControllerDeviceConfiguration::getIdealHostConfiguration() const
{
    // Fake a Dualsense controller
    BLEHostConfiguration config;

    // Explicitly set HID Device type
    config.setHidType(HID_GAMEPAD);

    // Vendor: Sony
    config.setVidSource(VENDOR_USB_SOURCE);
    config.setVid(DUALSENSE_VENDOR_ID);

    // Product: Dualsense Edge Wireless Controller
    config.setPid(DUALSENSE_EDGE_PRODUCT_ID);
    config.setGuidVersion(DUALSENSE_EDGE_BCD_DEVICE_ID);
    config.setSerialNumber(DUALSENSE_EDGE_SERIAL);
    config.setQueueSendRate(0);

    return config;
}

uint8_t DualsenseEdgeControllerDeviceConfiguration::getDeviceReportSize() const
{
    // Return the size of the device report

    return sizeof(DualsenseGamepadInputReportData); // 16
}

size_t DualsenseEdgeControllerDeviceConfiguration::makeDeviceReport(uint8_t* buffer, size_t bufferSize) const
{
    size_t hidDescriptorSize = sizeof(DualsenseEdge_HIDDescriptor);
    if (hidDescriptorSize < bufferSize) {
        memcpy(buffer, DualsenseEdge_HIDDescriptor, hidDescriptorSize);
    } else {
        return -1;
    }

    return hidDescriptorSize;
}

// -------------------------------------

// -------------------------------------
