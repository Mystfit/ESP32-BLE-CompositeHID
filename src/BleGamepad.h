#pragma once
// Unified include for BLE gamepad firmware.
// Pulls in the composite HID stack and all three specialized gamepad controllers
// — one #include is all a sketch needs.
#include "BleCompositeHID.h"
#include "BleConnectionStatus.h"
#include "gamepad/xbox/XboxGamepadDevice.h"
#include "gamepad/dualsense_edge/DualsenseGamepadDevice.h"
#include "gamepad/steam_controller_2026/SteamController2026GamepadDevice.h"
