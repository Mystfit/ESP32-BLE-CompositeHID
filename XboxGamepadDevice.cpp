#include "XboxGamepadDevice.h"


#if defined(CONFIG_ARDUHAL_ESP_LOG)
#include "esp32-hal-log.h"
#define LOG_TAG "GamepadDevice"
#else
#include "esp_log.h"
static const char *LOG_TAG = "XboxGamepadDevice";
#endif

XboxGamepadCallbacks::XboxGamepadCallbacks(XboxGamepadDevice* device) : _device(device)
{
}

void XboxGamepadCallbacks::onWrite(NimBLECharacteristic* pCharacteristic)
{
    ESP_LOGI(LOG_TAG, "XboxGamepadCallbacks::onWrite, value: %s, size: %d", pCharacteristic->getValue().c_str(), pCharacteristic->getValue().size());
}

void XboxGamepadCallbacks::onRead(NimBLECharacteristic* pCharacteristic)
{
    ESP_LOGI(LOG_TAG, "XboxGamepadCallbacks::onRead");
}

void XboxGamepadCallbacks::onNotify(NimBLECharacteristic* pCharacteristic)
{
    ESP_LOGI(LOG_TAG, "XboxGamepadCallbacks::onNotify");
}

void XboxGamepadCallbacks::onStatus(NimBLECharacteristic* pCharacteristic, Status status, int code)
{
    ESP_LOGI(LOG_TAG, "XboxGamepadCallbacks::onStatus, status: %d, code: %d", status, code);
}

// XboxGamepadDeviceConfiguration methods
XboxGamepadDeviceConfiguration::XboxGamepadDeviceConfiguration(uint8_t reportId) : BaseCompositeDeviceConfiguration(reportId) {
    
}

uint8_t XboxGamepadDeviceConfiguration::getDeviceReportSize() {
    // Return the size of the device report
    
    // Input
    // 2 * 16bit (2 bytes) for X and Y inclusive                = 4 bytes
    // 2 * 16bit (2 bytes) for Z and Rz inclusive               = 4 bytes
    // 1 * 10bit for brake + 6bit padding (2 bytes)             = 2 bytes
    // 1 * 10bit for accelerator + 6bit padding (2 bytes)       = 2 bytes
    // 1 * 4bit for hat switch + 4bit padding (1 byte)          = 1 byte
    // 15 * 1bit for buttons + 1bit padding (2 bytes)           = 2 bytes
    // 1 * 1bit for record button + 7bit padding (1 byte)       = 1 byte
    return 16;

    // Additional input?
    // 1 * 1bit + 7bit padding (1 byte)                         = 1 byte

    // TODO: Split output size into seperate function
    // Output 
    // 1 * 4bit for DC Enable Actuators + 4bit padding (1 byte) = 1 byte
    // 4 * 8bit for Magnitude (4 bytes)                         = 4 bytes
    // 3 * 8bit for Duration, Start Delay, Loop Count (3 bytes) = 3 bytes
}

size_t XboxGamepadDeviceConfiguration::makeDeviceReport(uint8_t* buffer, size_t bufferSize) {
    ESP_LOGE(LOG_TAG, "Before xbox descriptor alloc");
    uint8_t hidDescriptor[] = {
/*
        // https://github.com/DJm00n/ControllersInfo/blob/master/xboxone/xboxone_model_1914_firmware_5_17_hid_report_descriptor.txt

        0x05, 0x01,                 // (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page 
        0x09, 0x05,                 // (LOCAL)  USAGE              0x00010005 Game Pad (Application Collection)  
        0xA1, 0x01,                 // (MAIN)   COLLECTION         0x01 Application (Usage=0x00010005: Page=Generic Desktop Page, Usage=Game Pad, Type=Application Collection)
        0x85, XBOX_INPUT_REPORT_ID,     // (GLOBAL) REPORT_ID          0x01 (1)  
        0x09, 0x01,                     // (LOCAL)  USAGE              0x00010001 Pointer (Physical Collection)  
        0xA1, 0x00,                     // (MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=Physical Collection)
        0x09, 0x30,                         // (LOCAL)  USAGE              0x00010030 X (Dynamic Value)  
        0x09, 0x31,                         // (LOCAL)  USAGE              0x00010031 Y (Dynamic Value)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0)  <-- Info: Consider replacing 15 00 with 14
        0x27, 0xFF, 0xFF, 0x00, 0x00,       // (GLOBAL) LOGICAL_MAXIMUM    0x0000FFFF (65535)  
        0x95, 0x02,                         // (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields  
        0x75, 0x10,                         // (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field  
        0x81, 0x02,                         // (MAIN)   INPUT              0x00000002 (2 fields x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0xC0,                           // (MAIN)   END_COLLECTION     Physical 
        0x09, 0x01,                     // (LOCAL)  USAGE              0x00010001 Pointer (Physical Collection)  
        0xA1, 0x00,                     // (MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=Physical Collection)
        0x09, 0x32,                         // (LOCAL)  USAGE              0x00010032 Z (Dynamic Value)  
        0x09, 0x35,                         // (LOCAL)  USAGE              0x00010035 Rz (Dynamic Value)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x27, 0xFF, 0xFF, 0x00, 0x00,       // (GLOBAL) LOGICAL_MAXIMUM    0x0000FFFF (65535) <-- Redundant: LOGICAL_MAXIMUM is already 65535 
        0x95, 0x02,                         // (GLOBAL) REPORT_COUNT       0x02 (2) Number of fields <-- Redundant: REPORT_COUNT is already 2 
        0x75, 0x10,                         // (GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16 
        0x81, 0x02,                         // (MAIN)   INPUT              0x00000002 (2 fields x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0xC0,                           // (MAIN)   END_COLLECTION Physical
        0x05, 0x02,                     // (GLOBAL) USAGE_PAGE         0x0002 Simulation Controls Page 
        0x09, 0xC5,                     // (LOCAL)  USAGE              0x000200C5 Brake (Dynamic Value)  
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x03,               // (GLOBAL) LOGICAL_MAXIMUM    0x03FF (1023)  
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields  
        0x75, 0x0A,                     // (GLOBAL) REPORT_SIZE        0x0A (10) Number of bits per field  
        0x81, 0x02,                     // (MAIN)   INPUT              0x00000002 (1 field x 10 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                     // (GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x06,                     // (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field  
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x81, 0x03,                     // (MAIN)   INPUT              0x00000003 (1 field x 6 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x05, 0x02,                     // (GLOBAL) USAGE_PAGE         0x0002 Simulation Controls Page <-- Redundant: USAGE_PAGE is already 0x0002
        0x09, 0xC4,                     // (LOCAL)  USAGE              0x000200C4 Accelerator (Dynamic Value)  
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x03,               // (GLOBAL) LOGICAL_MAXIMUM    0x03FF (1023)  
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x75, 0x0A,                     // (GLOBAL) REPORT_SIZE        0x0A (10) Number of bits per field  
        0x81, 0x02,                     // (MAIN)   INPUT              0x00000002 (1 field x 10 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                     // (GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x06,                     // (GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field  
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x81, 0x03,                     // (MAIN)   INPUT              0x00000003 (1 field x 6 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x05, 0x01,                     // (GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page 
        0x09, 0x39,                     // (LOCAL)  USAGE              0x00010039 Hat switch (Dynamic Value)  
        0x15, 0x01,                     // (GLOBAL) LOGICAL_MINIMUM    0x01 (1)  
        0x25, 0x08,                     // (GLOBAL) LOGICAL_MAXIMUM    0x08 (8)  
        0x35, 0x00,                     // (GLOBAL) PHYSICAL_MINIMUM   0x00 (0)  <-- Info: Consider replacing 35 00 with 34
        0x46, 0x3B, 0x01,               // (GLOBAL) PHYSICAL_MAXIMUM   0x013B (315)
        0x66, 0x14, 0x00,               // (GLOBAL) UNIT               0x0014 Rotation in degrees [1° units] (4=System=English Rotation, 1=Rotation=Degrees)  <-- Info: Consider replacing 66 1400 with 65 14
        0x75, 0x04,                     // (GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field  
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x81, 0x42,                     // (MAIN)   INPUT              0x00000042 (1 field x 4 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 1=Null 0=NonVolatile 0=Bitmap 
        0x75, 0x04,                     // (GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field <-- Redundant: REPORT_SIZE is already 4 
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0)  <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                     // (GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x35, 0x00,                     // (GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34
        0x45, 0x00,                     // (GLOBAL) PHYSICAL_MAXIMUM   0x00 (0)  <-- Info: Consider replacing 45 00 with 44
        0x65, 0x00,                     // (GLOBAL) UNIT               0x00 No unit (0=None)  <-- Info: Consider replacing 65 00 with 64
        0x81, 0x03,                     // (MAIN)   INPUT              0x00000003 (1 field x 4 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x05, 0x09,                     // (GLOBAL) USAGE_PAGE         0x0009 Button Page 
        0x19, 0x01,                     // (LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (Selector, On/Off Control, Momentary Control, or One Shot Control)  
        0x29, 0x0F,                     // (LOCAL)  USAGE_MAXIMUM      0x0009000F Button 15 (Selector, On/Off Control, Momentary Control, or One Shot Control)  
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                     // (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)  
        0x75, 0x01,                     // (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field  
        0x95, 0x0F,                     // (GLOBAL) REPORT_COUNT       0x0F (15) Number of fields  
        0x81, 0x02,                     // (MAIN)   INPUT              0x00000002 (15 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                     // (GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x01,                     // (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1 
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields  
        0x81, 0x03,                     // (MAIN)   INPUT              0x00000003 (1 field x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x05, 0x0C,                     // (GLOBAL) USAGE_PAGE         0x000C Consumer Device Page 
        0x0A, 0xB2, 0x00,               // (LOCAL)  USAGE              0x000C00B2 Record (On/Off Control)  <-- Info: Consider replacing 0A B200 with 09 B2
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                     // (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x75, 0x01,                     // (GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1 
        0x81, 0x02,                     // (MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x15, 0x00,                     // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                     // (GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x07,                     // (GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field  
        0x95, 0x01,                     // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x81, 0x03,                     // (MAIN)   INPUT              0x00000003 (1 field x 7 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x05, 0x0F,                     // (GLOBAL) USAGE_PAGE         0x000F Physical Interface Device Page 
        0x09, 0x21,                     // (LOCAL)  USAGE              0x000F0021 Set Effect Report (Logical Collection)  
        0x85, XBOX_OUTPUT_REPORT_ID,    // (GLOBAL) REPORT_ID          0x03 (3)  
        0xA1, 0x02,                     // (MAIN)   COLLECTION         0x02 Logical (Usage=0x000F0021: Page=Physical Interface Device Page, Usage=Set Effect Report, Type=Logical Collection)
        0x09, 0x97,                         // (LOCAL)  USAGE              0x000F0097 DC Enable Actuators (Selector)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                         // (GLOBAL) LOGICAL_MAXIMUM    0x01 (1)  
        0x75, 0x04,                         // (GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field  
        0x95, 0x01,                         // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x91, 0x02,                         // (MAIN)   OUTPUT             0x00000002 (1 field x 4 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                         // (GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x04,                         // (GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field <-- Redundant: REPORT_SIZE is already 4 
        0x95, 0x01,                         // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x91, 0x03,                         // (MAIN)   OUTPUT             0x00000003 (1 field x 4 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x09, 0x70,                         // (LOCAL)  USAGE              0x000F0070 Magnitude (Dynamic Value)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x64,                         // (GLOBAL) LOGICAL_MAXIMUM    0x64 (100) 
        0x75, 0x08,                         // (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field  
        0x95, 0x04,                         // (GLOBAL) REPORT_COUNT       0x04 (4) Number of fields  
        0x91, 0x02,                         // (MAIN)   OUTPUT             0x00000002 (4 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x09, 0x50,                         // (LOCAL)  USAGE              0x000F0050 Duration (Dynamic Value)  
        0x66, 0x01, 0x10,                   // (GLOBAL) UNIT               0x1001 Time in seconds [1 s units] (1=System=SI Linear, 1=Time=Seconds)  
        0x55, 0x0E,                         // (GLOBAL) UNIT_EXPONENT      0x0E (Unit Value x 10⁻²)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                   // (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)  
        0x75, 0x08,                         // (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8 
        0x95, 0x01,                         // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields  
        0x91, 0x02,                         // (MAIN)   OUTPUT             0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x09, 0xA7,                         // (LOCAL)  USAGE              0x000F00A7 Start Delay (Dynamic Value)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                   // (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255) <-- Redundant: LOGICAL_MAXIMUM is already 255 
        0x75, 0x08,                         // (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8 
        0x95, 0x01,                         // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x91, 0x02,                         // (MAIN)   OUTPUT             0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0x65, 0x00,                         // (GLOBAL) UNIT               0x00 No unit (0=None)  <-- Info: Consider replacing 65 00 with 64
        0x55, 0x00,                         // (GLOBAL) UNIT_EXPONENT      0x00 (Unit Value x 10⁰)  <-- Info: Consider replacing 55 00 with 54
        0x09, 0x7C,                         // (LOCAL)  USAGE              0x000F007C Loop Count (Dynamic Value)  
        0x15, 0x00,                         // (GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                   // (GLOBAL) LOGICAL_MAXIMUM    0x00FF (255) <-- Redundant: LOGICAL_MAXIMUM is already 255 
        0x75, 0x08,                         // (GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8 
        0x95, 0x01,                         // (GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1 
        0x91, 0x02,                         // (MAIN)   OUTPUT             0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap 
        0xC0,                           // (MAIN)   END_COLLECTION     Logical 
        0xC0                        // (MAIN)   END_COLLECTION     Application 
    
*/

        // https://github.com/DJm00n/ControllersInfo/blob/master/xboxone/xboxone_model_1708_bluetooth_hid_report_descriptor.txt
        
        // ------ Input Report ------
        0x05, 0x01,                     //(GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page
        0x09, 0x05,                     //(LOCAL)  USAGE              0x00010005 Game Pad (Application Collection)
        0xA1, 0x01,                     //(MAIN)   COLLECTION         0x01 Application (Usage=0x00010005: Page=Generic Desktop Page, Usage=Game Pad, Type=Application Collection)
        0x85, XBOX_INPUT_REPORT_ID,         //(GLOBAL) REPORT_ID          0x01 (1)
        0x09, 0x01,                         //(LOCAL)  USAGE              0x00010001 Pointer (Physical Collection)
        0xA1, 0x00,                         //(MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=Physical Collection)
        0x09, 0x30,                             //(LOCAL)  USAGE              0x00010030 X (Dynamic Value)
        0x09, 0x31,                             //(LOCAL)  USAGE              0x00010031 Y (Dynamic Value)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0)  <-- Info: Consider replacing 15 00 with 14
        0x27, 0xFF, 0xFF, 0x00, 0x00,           //(GLOBAL) LOGICAL_MAXIMUM    0x0000FFFF (65535)
        0x95, 0x02,                             //(GLOBAL) REPORT_COUNT       0x02 (2) Number of fields
        0x75, 0x10,                             //(GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field
        0x81, 0x02,                             //(MAIN)   INPUT              0x00000002 (2 fields x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0xC0,                               //(MAIN)   END_COLLECTION     Physical
        0x09, 0x01,                         //(LOCAL)  USAGE              0x00010001 Pointer (Physical Collection)
        0xA1, 0x00,                         //(MAIN)   COLLECTION         0x00 Physical (Usage=0x00010001: Page=Generic Desktop Page, Usage=Pointer, Type=Physical Collection)
        0x09, 0x32,                             //(LOCAL)  USAGE              0x00010032 Z (Dynamic Value)
        0x09, 0x35,                             //(LOCAL)  USAGE              0x00010035 Rz (Dynamic Value)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x27, 0xFF, 0xFF, 0x00, 0x00,           //(GLOBAL) LOGICAL_MAXIMUM    0x0000FFFF (65535) <-- Redundant: LOGICAL_MAXIMUM is already 65535
        0x95, 0x02,                             //(GLOBAL) REPORT_COUNT       0x02 (2) Number of fields <-- Redundant: REPORT_COUNT is already 2
        0x75, 0x10,                             //(GLOBAL) REPORT_SIZE        0x10 (16) Number of bits per field <-- Redundant: REPORT_SIZE is already 16
        0x81, 0x02,                             //(MAIN)   INPUT              0x00000002 (2 fields x 16 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0xC0,                               //(MAIN)   END_COLLECTION     Physical
        0x05, 0x02,                         //(GLOBAL) USAGE_PAGE         0x0002 Simulation Controls Page
        0x09, 0xC5,                         //(LOCAL)  USAGE              0x000200C5 Brake (Dynamic Value)
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x03,                   //(GLOBAL) LOGICAL_MAXIMUM    0x03FF (1023)
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields
        0x75, 0x0A,                         //(GLOBAL) REPORT_SIZE        0x0A (10) Number of bits per field
        0x81, 0x02,                         //(MAIN)   INPUT              0x00000002 (1 field x 10 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                         //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x06,                         //(GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x81, 0x03,                         //(MAIN)   INPUT              0x00000003 (1 field x 6 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x05, 0x02,                         //(GLOBAL) USAGE_PAGE         0x0002 Simulation Controls Page <-- Redundant: USAGE_PAGE is already 0x0002
        0x09, 0xC4,                         //(LOCAL)  USAGE              0x000200C4 Accelerator (Dynamic Value)
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x03,                   //(GLOBAL) LOGICAL_MAXIMUM    0x03FF (1023)
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x75, 0x0A,                         //(GLOBAL) REPORT_SIZE        0x0A (10) Number of bits per field
        0x81, 0x02,                         //(MAIN)   INPUT              0x00000002 (1 field x 10 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                         //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x06,                         //(GLOBAL) REPORT_SIZE        0x06 (6) Number of bits per field
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x81, 0x03,                         //(MAIN)   INPUT              0x00000003 (1 field x 6 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x05, 0x01,                         //(GLOBAL) USAGE_PAGE         0x0001 Generic Desktop Page
        0x09, 0x39,                         //(LOCAL)  USAGE              0x00010039 Hat switch (Dynamic Value)
        0x15, 0x01,                         //(GLOBAL) LOGICAL_MINIMUM    0x01 (1)
        0x25, 0x08,                         //(GLOBAL) LOGICAL_MAXIMUM    0x08 (8)
        0x35, 0x00,                         //(GLOBAL) PHYSICAL_MINIMUM   0x00 (0)  <-- Info: Consider replacing 35 00 with 34
        0x46, 0x3B, 0x01,                   //(GLOBAL) PHYSICAL_MAXIMUM   0x013B (315)
        0x66, 0x14, 0x00,                   //(GLOBAL) UNIT               0x0014 Rotation in degrees [1° units] (4=System=English Rotation, 1=Rotation=Degrees)  <-- Info: Consider replacing 66 1400 with 65 14
        0x75, 0x04,                         //(GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x81, 0x42,                         //(MAIN)   INPUT              0x00000042 (1 field x 4 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 1=Null 0=NonVolatile 0=Bitmap
        0x75, 0x04,                         //(GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field <-- Redundant: REPORT_SIZE is already 4
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0)  <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                         //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x35, 0x00,                         //(GLOBAL) PHYSICAL_MINIMUM   0x00 (0) <-- Redundant: PHYSICAL_MINIMUM is already 0 <-- Info: Consider replacing 35 00 with 34
        0x45, 0x00,                         //(GLOBAL) PHYSICAL_MAXIMUM   0x00 (0)  <-- Info: Consider replacing 45 00 with 44
        0x65, 0x00,                         //(GLOBAL) UNIT               0x00 No unit (0=None)  <-- Info: Consider replacing 65 00 with 64
        0x81, 0x03,                         //(MAIN)   INPUT              0x00000003 (1 field x 4 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x05, 0x09,                         //(GLOBAL) USAGE_PAGE         0x0009 Button Page
        0x19, 0x01,                         //(LOCAL)  USAGE_MINIMUM      0x00090001 Button 1 Primary/trigger (Selector, On/Off Control, Momentary Control, or One Shot Control)
        0x29, 0x0F,                         //(LOCAL)  USAGE_MAXIMUM      0x0009000F Button 15 (Selector, On/Off Control, Momentary Control, or One Shot Control)
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                         //(GLOBAL) LOGICAL_MAXIMUM    0x01 (1)
        0x75, 0x01,                         //(GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field
        0x95, 0x0F,                         //(GLOBAL) REPORT_COUNT       0x0F (15) Number of fields
        0x81, 0x02,                         //(MAIN)   INPUT              0x00000002 (15 fields x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                         //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x01,                         //(GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields
        0x81, 0x03,                         //(MAIN)   INPUT              0x00000003 (1 field x 1 bit) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x05, 0x0C,                         //(GLOBAL) USAGE_PAGE         0x000C Consumer Device Page
        0x0A, 0x24, 0x02,                   //(LOCAL)  USAGE              0x000C0224 AC Back (Selector)
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                         //(GLOBAL) LOGICAL_MAXIMUM    0x01 (1)
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x75, 0x01,                         //(GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field <-- Redundant: REPORT_SIZE is already 1
        0x81, 0x02,                         //(MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                         //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x07,                         //(GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x81, 0x03,                         //(MAIN)   INPUT              0x00000003 (1 field x 7 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        
        // ----- Input report additional?  -------
        0x05, 0x0C,                         //(GLOBAL) USAGE_PAGE         0x000C Consumer Device Page <-- Redundant: USAGE_PAGE is already 0x000C
        0x09, 0x01,                         //(LOCAL)  USAGE              0x000C0001 Consumer Control (Application Collection)
        0x85, XBOX_EXTRA_INPUT_REPORT_ID,   //(GLOBAL) REPORT_ID          0x02 (2)
        0xA1, 0x01,                         //(MAIN)   COLLECTION         0x01 Application (Usage=0x000C0001: Page=Consumer Device Page, Usage=Consumer Control, Type=Application Collection)
        0x05, 0x0C,                             //(GLOBAL) USAGE_PAGE         0x000C Consumer Device Page <-- Redundant: USAGE_PAGE is already 0x000C
        0x0A, 0x23, 0x02,                       //(LOCAL)  USAGE              0x000C0223 AC Home (Selector)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                             //(GLOBAL) LOGICAL_MAXIMUM    0x01 (1)
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x75, 0x01,                             //(GLOBAL) REPORT_SIZE        0x01 (1) Number of bits per field
        0x81, 0x02,                             //(MAIN)   INPUT              0x00000002 (1 field x 1 bit) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                             //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x07,                             //(GLOBAL) REPORT_SIZE        0x07 (7) Number of bits per field
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x81, 0x03,                             //(MAIN)   INPUT              0x00000003 (1 field x 7 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0xC0,                               //(MAIN)   END_COLLECTION     Application
        
        // ----- Output Report -----
        0x05, 0x0F,                         //(GLOBAL) USAGE_PAGE         0x000F Physical Interface Device Page
        0x09, 0x21,                         //(LOCAL)  USAGE              0x000F0021 Set Effect Report (Logical Collection)
        0x85, XBOX_OUTPUT_REPORT_ID,        //(GLOBAL) REPORT_ID          0x03 (3)
        0xA1, 0x02,                         //(MAIN)   COLLECTION         0x02 Logical (Usage=0x000F0021: Page=Physical Interface Device Page, Usage=Set Effect Report, Type=Logical Collection)
        0x09, 0x97,                             //(LOCAL)  USAGE              0x000F0097 DC Enable Actuators (Selector)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x01,                             //(GLOBAL) LOGICAL_MAXIMUM    0x01 (1)
        0x75, 0x04,                             //(GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x91, 0x02,                             //(MAIN)   OUTPUT             0x00000002 (1 field x 4 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x00,                             //(GLOBAL) LOGICAL_MAXIMUM    0x00 (0)  <-- Info: Consider replacing 25 00 with 24
        0x75, 0x04,                             //(GLOBAL) REPORT_SIZE        0x04 (4) Number of bits per field <-- Redundant: REPORT_SIZE is already 4
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x91, 0x03,                             //(MAIN)   OUTPUT             0x00000003 (1 field x 4 bits) 1=Constant 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x09, 0x70,                             //(LOCAL)  USAGE              0x000F0070 Magnitude (Dynamic Value)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x25, 0x64,                             //(GLOBAL) LOGICAL_MAXIMUM    0x64 (100)
        0x75, 0x08,                             //(GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field
        0x95, 0x04,                             //(GLOBAL) REPORT_COUNT       0x04 (4) Number of fields
        0x91, 0x02,                             //(MAIN)   OUTPUT             0x00000002 (4 fields x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x09, 0x50,                             //(LOCAL)  USAGE              0x000F0050 Duration (Dynamic Value)
        0x66, 0x01, 0x10,                       //(GLOBAL) UNIT               0x1001 Time in seconds [1 s units] (1=System=SI Linear, 1=Time=Seconds)
        0x55, 0x0E,                             //(GLOBAL) UNIT_EXPONENT      0x0E (Unit Value x 10⁻²)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                       //(GLOBAL) LOGICAL_MAXIMUM    0x00FF (255)
        0x75, 0x08,                             //(GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields
        0x91, 0x02,                             //(MAIN)   OUTPUT             0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x09, 0xA7,                             //(LOCAL)  USAGE              0x000F00A7 Start Delay (Dynamic Value)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                       //(GLOBAL) LOGICAL_MAXIMUM    0x00FF (255) <-- Redundant: LOGICAL_MAXIMUM is already 255
        0x75, 0x08,                             //(GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x91, 0x02,                             //(MAIN)   OUTPUT             0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0x65, 0x00,                             //(GLOBAL) UNIT               0x00 No unit (0=None)  <-- Info: Consider replacing 65 00 with 64
        0x55, 0x00,                             //(GLOBAL) UNIT_EXPONENT      0x00 (Unit Value x 10⁰)  <-- Info: Consider replacing 55 00 with 54
        0x09, 0x7C,                             //(LOCAL)  USAGE              0x000F007C Loop Count (Dynamic Value)
        0x15, 0x00,                             //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                       //(GLOBAL) LOGICAL_MAXIMUM    0x00FF (255) <-- Redundant: LOGICAL_MAXIMUM is already 255
        0x75, 0x08,                             //(GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8
        0x95, 0x01,                             //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x91, 0x02,                             //(MAIN)   OUTPUT             0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0xC0,                               //(MAIN)   END_COLLECTION     Logical
        0x05, 0x06,                         //(GLOBAL) USAGE_PAGE         0x0006 Generic Device Controls Page
        0x09, 0x20,                         //(LOCAL)  USAGE              0x00060020 Battery Strength (Dynamic Value)
        0x85, 0x04,                         //(GLOBAL) REPORT_ID          0x04 (4)
        0x15, 0x00,                         //(GLOBAL) LOGICAL_MINIMUM    0x00 (0) <-- Redundant: LOGICAL_MINIMUM is already 0 <-- Info: Consider replacing 15 00 with 14
        0x26, 0xFF, 0x00,                   //(GLOBAL) LOGICAL_MAXIMUM    0x00FF (255) <-- Redundant: LOGICAL_MAXIMUM is already 255
        0x75, 0x08,                         //(GLOBAL) REPORT_SIZE        0x08 (8) Number of bits per field <-- Redundant: REPORT_SIZE is already 8
        0x95, 0x01,                         //(GLOBAL) REPORT_COUNT       0x01 (1) Number of fields <-- Redundant: REPORT_COUNT is already 1
        0x81, 0x02,                         //(MAIN)   INPUT              0x00000002 (1 field x 8 bits) 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
        0xC0                            //(MAIN)   END_COLLECTION     Application
    };

    if(sizeof(hidDescriptor) < bufferSize){
        memcpy(buffer, hidDescriptor, sizeof(hidDescriptor));
    } else {
        return -1;
    }
    
    return sizeof(hidDescriptor);
}

// XboxGamepadDevice methods
XboxGamepadDevice::XboxGamepadDevice(const XboxGamepadDeviceConfiguration& config) : _config(config)
{
}

XboxGamepadDevice::~XboxGamepadDevice() {
    if (getOutput() && _callbacks){
        getOutput()->setCallbacks(nullptr);
        delete _callbacks;
    }

    if(_extra_input){
        delete _extra_input;
    }
}

void XboxGamepadDevice::init(NimBLEHIDDevice* hid) {
    /// Create input characteristic to send events to the computer
    auto input = hid->inputReport(XBOX_INPUT_REPORT_ID);
    _extra_input = hid->inputReport(XBOX_EXTRA_INPUT_REPORT_ID);

    // Create output characteristic to handle events coming from the computer
    auto output = hid->outputReport(XBOX_OUTPUT_REPORT_ID);
    _callbacks = new XboxGamepadCallbacks(this);
    output->setCallbacks(_callbacks);

    setCharacteristics(input, output);
}

BaseCompositeDeviceConfiguration* XboxGamepadDevice::getDeviceConfig() 
{
    // Return the device configuration
    return &_config;
}