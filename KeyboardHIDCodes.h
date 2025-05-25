/**
 * USB HID Keyboard scan codes as per USB spec 1.11
 * plus some additional codes
 * 
 * Created by MightyPork, 2016
 * Public domain
 * 
 * Adapted from:
 * https://source.android.com/devices/input/keyboard-devices.html
 */

#ifndef USB_HID_KEYS
#define USB_HID_KEYS

#ifndef _TUSB_HID_H_
//--------------------------------------------------------------------+
// HID KEYCODE
//--------------------------------------------------------------------+
#define HID_KEY_NONE                        0x00
#define HID_KEY_A                           0x04
#define HID_KEY_B                           0x05
#define HID_KEY_C                           0x06
#define HID_KEY_D                           0x07
#define HID_KEY_E                           0x08
#define HID_KEY_F                           0x09
#define HID_KEY_G                           0x0A
#define HID_KEY_H                           0x0B
#define HID_KEY_I                           0x0C
#define HID_KEY_J                           0x0D
#define HID_KEY_K                           0x0E
#define HID_KEY_L                           0x0F
#define HID_KEY_M                           0x10
#define HID_KEY_N                           0x11
#define HID_KEY_O                           0x12
#define HID_KEY_P                           0x13
#define HID_KEY_Q                           0x14
#define HID_KEY_R                           0x15
#define HID_KEY_S                           0x16
#define HID_KEY_T                           0x17
#define HID_KEY_U                           0x18
#define HID_KEY_V                           0x19
#define HID_KEY_W                           0x1A
#define HID_KEY_X                           0x1B
#define HID_KEY_Y                           0x1C
#define HID_KEY_Z                           0x1D
#define HID_KEY_1                           0x1E
#define HID_KEY_2                           0x1F
#define HID_KEY_3                           0x20
#define HID_KEY_4                           0x21
#define HID_KEY_5                           0x22
#define HID_KEY_6                           0x23
#define HID_KEY_7                           0x24
#define HID_KEY_8                           0x25
#define HID_KEY_9                           0x26
#define HID_KEY_0                           0x27
#define HID_KEY_ENTER                       0x28
#define HID_KEY_ESCAPE                      0x29
#define HID_KEY_BACKSPACE                   0x2A
#define HID_KEY_TAB                         0x2B
#define HID_KEY_SPACE                       0x2C
#define HID_KEY_MINUS                       0x2D
#define HID_KEY_EQUAL                       0x2E
#define HID_KEY_BRACKET_LEFT                0x2F
#define HID_KEY_BRACKET_RIGHT               0x30
#define HID_KEY_BACKSLASH                   0x31
#define HID_KEY_EUROPE_1                    0x32
#define HID_KEY_SEMICOLON                   0x33
#define HID_KEY_APOSTROPHE                  0x34
#define HID_KEY_GRAVE                       0x35
#define HID_KEY_COMMA                       0x36
#define HID_KEY_PERIOD                      0x37
#define HID_KEY_SLASH                       0x38
#define HID_KEY_CAPS_LOCK                   0x39
#define HID_KEY_F1                          0x3A
#define HID_KEY_F2                          0x3B
#define HID_KEY_F3                          0x3C
#define HID_KEY_F4                          0x3D
#define HID_KEY_F5                          0x3E
#define HID_KEY_F6                          0x3F
#define HID_KEY_F7                          0x40
#define HID_KEY_F8                          0x41
#define HID_KEY_F9                          0x42
#define HID_KEY_F10                         0x43
#define HID_KEY_F11                         0x44
#define HID_KEY_F12                         0x45
#define HID_KEY_PRINT_SCREEN                0x46
#define HID_KEY_SCROLL_LOCK                 0x47
#define HID_KEY_PAUSE                       0x48
#define HID_KEY_INSERT                      0x49
#define HID_KEY_HOME                        0x4A
#define HID_KEY_PAGE_UP                     0x4B
#define HID_KEY_DELETE                      0x4C
#define HID_KEY_END                         0x4D
#define HID_KEY_PAGE_DOWN                   0x4E
#define HID_KEY_ARROW_RIGHT                 0x4F
#define HID_KEY_ARROW_LEFT                  0x50
#define HID_KEY_ARROW_DOWN                  0x51
#define HID_KEY_ARROW_UP                    0x52
#define HID_KEY_NUM_LOCK                    0x53
#define HID_KEY_KEYPAD_DIVIDE               0x54
#define HID_KEY_KEYPAD_MULTIPLY             0x55
#define HID_KEY_KEYPAD_SUBTRACT             0x56
#define HID_KEY_KEYPAD_ADD                  0x57
#define HID_KEY_KEYPAD_ENTER                0x58
#define HID_KEY_KEYPAD_1                    0x59
#define HID_KEY_KEYPAD_2                    0x5A
#define HID_KEY_KEYPAD_3                    0x5B
#define HID_KEY_KEYPAD_4                    0x5C
#define HID_KEY_KEYPAD_5                    0x5D
#define HID_KEY_KEYPAD_6                    0x5E
#define HID_KEY_KEYPAD_7                    0x5F
#define HID_KEY_KEYPAD_8                    0x60
#define HID_KEY_KEYPAD_9                    0x61
#define HID_KEY_KEYPAD_0                    0x62
#define HID_KEY_KEYPAD_DECIMAL              0x63
#define HID_KEY_EUROPE_2                    0x64
#define HID_KEY_APPLICATION                 0x65
#define HID_KEY_POWER                       0x66
#define HID_KEY_KEYPAD_EQUAL                0x67
#define HID_KEY_F13                         0x68
#define HID_KEY_F14                         0x69
#define HID_KEY_F15                         0x6A
#define HID_KEY_F16                         0x6B
#define HID_KEY_F17                         0x6C
#define HID_KEY_F18                         0x6D
#define HID_KEY_F19                         0x6E
#define HID_KEY_F20                         0x6F
#define HID_KEY_F21                         0x70
#define HID_KEY_F22                         0x71
#define HID_KEY_F23                         0x72
#define HID_KEY_F24                         0x73
#define HID_KEY_EXECUTE                     0x74
#define HID_KEY_HELP                        0x75
#define HID_KEY_MENU                        0x76
#define HID_KEY_SELECT                      0x77
#define HID_KEY_STOP                        0x78
#define HID_KEY_AGAIN                       0x79
#define HID_KEY_UNDO                        0x7A
#define HID_KEY_CUT                         0x7B
#define HID_KEY_COPY                        0x7C
#define HID_KEY_PASTE                       0x7D
#define HID_KEY_FIND                        0x7E
#define HID_KEY_MUTE                        0x7F
#define HID_KEY_VOLUME_UP                   0x80
#define HID_KEY_VOLUME_DOWN                 0x81
#define HID_KEY_LOCKING_CAPS_LOCK           0x82
#define HID_KEY_LOCKING_NUM_LOCK            0x83
#define HID_KEY_LOCKING_SCROLL_LOCK         0x84
#define HID_KEY_KEYPAD_COMMA                0x85
#define HID_KEY_KEYPAD_EQUAL_SIGN           0x86
#define HID_KEY_KANJI1                      0x87
#define HID_KEY_KANJI2                      0x88
#define HID_KEY_KANJI3                      0x89
#define HID_KEY_KANJI4                      0x8A
#define HID_KEY_KANJI5                      0x8B
#define HID_KEY_KANJI6                      0x8C
#define HID_KEY_KANJI7                      0x8D
#define HID_KEY_KANJI8                      0x8E
#define HID_KEY_KANJI9                      0x8F
#define HID_KEY_LANG1                       0x90
#define HID_KEY_LANG2                       0x91
#define HID_KEY_LANG3                       0x92
#define HID_KEY_LANG4                       0x93
#define HID_KEY_LANG5                       0x94
#define HID_KEY_LANG6                       0x95
#define HID_KEY_LANG7                       0x96
#define HID_KEY_LANG8                       0x97
#define HID_KEY_LANG9                       0x98
#define HID_KEY_ALTERNATE_ERASE             0x99
#define HID_KEY_SYSREQ_ATTENTION            0x9A
#define HID_KEY_CANCEL                      0x9B
#define HID_KEY_CLEAR                       0x9C
#define HID_KEY_PRIOR                       0x9D
#define HID_KEY_RETURN                      0x9E
#define HID_KEY_SEPARATOR                   0x9F
#define HID_KEY_OUT                         0xA0
#define HID_KEY_OPER                        0xA1
#define HID_KEY_CLEAR_AGAIN                 0xA2
#define HID_KEY_CRSEL_PROPS                 0xA3
#define HID_KEY_EXSEL                       0xA4
// RESERVED					                        0xA5-AF
#define HID_KEY_KEYPAD_00                   0xB0
#define HID_KEY_KEYPAD_000                  0xB1
#define HID_KEY_THOUSANDS_SEPARATOR         0xB2
#define HID_KEY_DECIMAL_SEPARATOR           0xB3
#define HID_KEY_CURRENCY_UNIT               0xB4
#define HID_KEY_CURRENCY_SUBUNIT            0xB5
#define HID_KEY_KEYPAD_LEFT_PARENTHESIS     0xB6
#define HID_KEY_KEYPAD_RIGHT_PARENTHESIS    0xB7
#define HID_KEY_KEYPAD_LEFT_BRACE           0xB8
#define HID_KEY_KEYPAD_RIGHT_BRACE          0xB9
#define HID_KEY_KEYPAD_TAB                  0xBA
#define HID_KEY_KEYPAD_BACKSPACE            0xBB
#define HID_KEY_KEYPAD_A                    0xBC
#define HID_KEY_KEYPAD_B                    0xBD
#define HID_KEY_KEYPAD_C                    0xBE
#define HID_KEY_KEYPAD_D                    0xBF
#define HID_KEY_KEYPAD_E                    0xC0
#define HID_KEY_KEYPAD_F                    0xC1
#define HID_KEY_KEYPAD_XOR                  0xC2
#define HID_KEY_KEYPAD_CARET                0xC3
#define HID_KEY_KEYPAD_PERCENT              0xC4
#define HID_KEY_KEYPAD_LESS_THAN            0xC5
#define HID_KEY_KEYPAD_GREATER_THAN         0xC6
#define HID_KEY_KEYPAD_AMPERSAND            0xC7
#define HID_KEY_KEYPAD_DOUBLE_AMPERSAND     0xC8
#define HID_KEY_KEYPAD_VERTICAL_BAR         0xC9
#define HID_KEY_KEYPAD_DOUBLE_VERTICAL_BAR  0xCA
#define HID_KEY_KEYPAD_COLON                0xCB
#define HID_KEY_KEYPAD_HASH                 0xCC
#define HID_KEY_KEYPAD_SPACE                0xCD
#define HID_KEY_KEYPAD_AT                   0xCE
#define HID_KEY_KEYPAD_EXCLAMATION          0xCF
#define HID_KEY_KEYPAD_MEMORY_STORE         0xD0
#define HID_KEY_KEYPAD_MEMORY_RECALL        0xD1
#define HID_KEY_KEYPAD_MEMORY_CLEAR         0xD2
#define HID_KEY_KEYPAD_MEMORY_ADD           0xD3
#define HID_KEY_KEYPAD_MEMORY_SUBTRACT      0xD4
#define HID_KEY_KEYPAD_MEMORY_MULTIPLY      0xD5
#define HID_KEY_KEYPAD_MEMORY_DIVIDE        0xD6
#define HID_KEY_KEYPAD_PLUS_MINUS           0xD7
#define HID_KEY_KEYPAD_CLEAR                0xD8
#define HID_KEY_KEYPAD_CLEAR_ENTRY          0xD9
#define HID_KEY_KEYPAD_BINARY               0xDA
#define HID_KEY_KEYPAD_OCTAL                0xDB
#define HID_KEY_KEYPAD_DECIMAL_2            0xDC
#define HID_KEY_KEYPAD_HEXADECIMAL          0xDD
// RESERVED					                        0xDE-DF
#define HID_KEY_CONTROL_LEFT                0xE0
#define HID_KEY_SHIFT_LEFT                  0xE1
#define HID_KEY_ALT_LEFT                    0xE2
#define HID_KEY_GUI_LEFT                    0xE3
#define HID_KEY_CONTROL_RIGHT               0xE4
#define HID_KEY_SHIFT_RIGHT                 0xE5
#define HID_KEY_ALT_RIGHT                   0xE6
#define HID_KEY_GUI_RIGHT                   0xE7

#endif // _TUSB_HID_H_
#ifndef CONFIG_TINYUSB_HID_ENABLED
// from https://github.com/espressif/arduino-esp32/blob/master/libraries/USB/src/USBHIDKeyboard.h
#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_RIGHT_CTRL  0x84
#define KEY_RIGHT_SHIFT 0x85
#define KEY_RIGHT_ALT   0x86  // AltGr (Right Alt) Key
#define KEY_RIGHT_GUI   0x87

#define KEY_UP_ARROW     0xDA
#define KEY_DOWN_ARROW   0xD9
#define KEY_LEFT_ARROW   0xD8
#define KEY_RIGHT_ARROW  0xD7
#define KEY_MENU         0xED  //  "Keyboard Application" in USB standard
#define KEY_SPACE        0x20
#define KEY_BACKSPACE    0xB2
#define KEY_TAB          0xB3
#define KEY_RETURN       0xB0
#define KEY_ESC          0xB1
#define KEY_INSERT       0xD1
#define KEY_DELETE       0xD4
#define KEY_PAGE_UP      0xD3
#define KEY_PAGE_DOWN    0xD6
#define KEY_HOME         0xD2
#define KEY_END          0xD5
#define KEY_NUM_LOCK     0xDB
#define KEY_CAPS_LOCK    0xC1
#define KEY_F1           0xC2
#define KEY_F2           0xC3
#define KEY_F3           0xC4
#define KEY_F4           0xC5
#define KEY_F5           0xC6
#define KEY_F6           0xC7
#define KEY_F7           0xC8
#define KEY_F8           0xC9
#define KEY_F9           0xCA
#define KEY_F10          0xCB
#define KEY_F11          0xCC
#define KEY_F12          0xCD
#define KEY_F13          0xF0
#define KEY_F14          0xF1
#define KEY_F15          0xF2
#define KEY_F16          0xF3
#define KEY_F17          0xF4
#define KEY_F18          0xF5
#define KEY_F19          0xF6
#define KEY_F20          0xF7
#define KEY_F21          0xF8
#define KEY_F22          0xF9
#define KEY_F23          0xFA
#define KEY_F24          0xFB
#define KEY_PRINT_SCREEN 0xCE
#define KEY_SCROLL_LOCK  0xCF
#define KEY_PAUSE        0xD0

#define LED_NUMLOCK    0x01
#define LED_CAPSLOCK   0x02
#define LED_SCROLLLOCK 0x04
#define LED_COMPOSE    0x08
#define LED_KANA       0x10

// Numeric keypad
#define KEY_KP_SLASH    0xDC
#define KEY_KP_ASTERISK 0xDD
#define KEY_KP_MINUS    0xDE
#define KEY_KP_PLUS     0xDF
#define KEY_KP_ENTER    0xE0
#define KEY_KP_1        0xE1
#define KEY_KP_2        0xE2
#define KEY_KP_3        0xE3
#define KEY_KP_4        0xE4
#define KEY_KP_5        0xE5
#define KEY_KP_6        0xE6
#define KEY_KP_7        0xE7
#define KEY_KP_8        0xE8
#define KEY_KP_9        0xE9
#define KEY_KP_0        0xEA
#define KEY_KP_DOT      0xEB
#endif
/**
 * Scan codes - last N slots in the HID report (usually 6).
 * 0x00 if no key pressed.
 * 
 * If more than N keys are pressed, the HID reports 
 * KEY_ERR_OVF in all slots to indicate this condition.
 */

#define KEY_NONE 0x00 // No key pressed
#define KEY_ERR_OVF 0x01 //  Keyboard Error Roll Over - used for all slots if too many keys are pressed ("Phantom key")
// 0x02 //  Keyboard POST Fail
// 0x03 //  Keyboard Error Undefined
#define KEY_A 0x04 // Keyboard a and A
#define KEY_B 0x05 // Keyboard b and B
#define KEY_C 0x06 // Keyboard c and C
#define KEY_D 0x07 // Keyboard d and D
#define KEY_E 0x08 // Keyboard e and E
#define KEY_F 0x09 // Keyboard f and F
#define KEY_G 0x0a // Keyboard g and G
#define KEY_H 0x0b // Keyboard h and H
#define KEY_I 0x0c // Keyboard i and I
#define KEY_J 0x0d // Keyboard j and J
#define KEY_K 0x0e // Keyboard k and K
#define KEY_L 0x0f // Keyboard l and L
#define KEY_M 0x10 // Keyboard m and M
#define KEY_N 0x11 // Keyboard n and N
#define KEY_O 0x12 // Keyboard o and O
#define KEY_P 0x13 // Keyboard p and P
#define KEY_Q 0x14 // Keyboard q and Q
#define KEY_R 0x15 // Keyboard r and R
#define KEY_S 0x16 // Keyboard s and S
#define KEY_T 0x17 // Keyboard t and T
#define KEY_U 0x18 // Keyboard u and U
#define KEY_V 0x19 // Keyboard v and V
#define KEY_W 0x1a // Keyboard w and W
#define KEY_X 0x1b // Keyboard x and X
#define KEY_Y 0x1c // Keyboard y and Y
#define KEY_Z 0x1d // Keyboard z and Z

#define KEY_1 0x1e // Keyboard 1 and !
#define KEY_2 0x1f // Keyboard 2 and @
#define KEY_3 0x20 // Keyboard 3 and #
#define KEY_4 0x21 // Keyboard 4 and $
#define KEY_5 0x22 // Keyboard 5 and %
#define KEY_6 0x23 // Keyboard 6 and ^
#define KEY_7 0x24 // Keyboard 7 and &
#define KEY_8 0x25 // Keyboard 8 and *
#define KEY_9 0x26 // Keyboard 9 and (
#define KEY_0 0x27 // Keyboard 0 and )

#define KEY_ENTER 0x28 // Keyboard Return (ENTER)
//#define KEY_ESC 0x29 // Keyboard ESCAPE
//#define KEY_BACKSPACE 0x2a // Keyboard DELETE (Backspace)
//#define KEY_TAB 0x2b // Keyboard Tab
//#define KEY_SPACE 0x2c // Keyboard Spacebar
#define KEY_MINUS 0x2d // Keyboard - and _
#define KEY_EQUAL 0x2e // Keyboard = and +
#define KEY_LEFTBRACE 0x2f // Keyboard [ and {
#define KEY_RIGHTBRACE 0x30 // Keyboard ] and }
#define KEY_BACKSLASH 0x31 // Keyboard \ and |
#define KEY_HASHTILDE 0x32 // Keyboard Non-US # and ~
#define KEY_SEMICOLON 0x33 // Keyboard ; and :
#define KEY_APOSTROPHE 0x34 // Keyboard ' and "
#define KEY_GRAVE 0x35 // Keyboard ` and ~
#define KEY_COMMA 0x36 // Keyboard , and <
#define KEY_DOT 0x37 // Keyboard . and >
#define KEY_SLASH 0x38 // Keyboard / and ?
#define KEY_CAPSLOCK 0x39 // Keyboard Caps Lock

//#define KEY_F1 0x3a // Keyboard F1
//#define KEY_F2 0x3b // Keyboard F2
//#define KEY_F3 0x3c // Keyboard F3
//#define KEY_F4 0x3d // Keyboard F4
//#define KEY_F5 0x3e // Keyboard F5
//#define KEY_F6 0x3f // Keyboard F6
//#define KEY_F7 0x40 // Keyboard F7
//#define KEY_F8 0x41 // Keyboard F8
//#define KEY_F9 0x42 // Keyboard F9
//#define KEY_F10 0x43 // Keyboard F10
//#define KEY_F11 0x44 // Keyboard F11
//#define KEY_F12 0x45 // Keyboard F12

#define KEY_SYSRQ 0x46 // Keyboard Print Screen
#define KEY_SCROLLLOCK 0x47 // Keyboard Scroll Lock
//#define KEY_PAUSE 0x48 // Keyboard Pause
//#define KEY_INSERT 0x49 // Keyboard Insert
//#define KEY_HOME 0x4a // Keyboard Home
#define KEY_PAGEUP 0x4b // Keyboard Page Up
//#define KEY_DELETE 0x4c // Keyboard Delete Forward
//#define KEY_END 0x4d // Keyboard End
#define KEY_PAGEDOWN 0x4e // Keyboard Page Down
#define KEY_RIGHT 0x4f // Keyboard Right Arrow
#define KEY_LEFT 0x50 // Keyboard Left Arrow
#define KEY_DOWN 0x51 // Keyboard Down Arrow
#define KEY_UP 0x52 // Keyboard Up Arrow

#define KEY_NUMLOCK 0x53 // Keyboard Num Lock and Clear
#define KEY_KPSLASH 0x54 // Keypad /
#define KEY_KPASTERISK 0x55 // Keypad *
#define KEY_KPMINUS 0x56 // Keypad -
#define KEY_KPPLUS 0x57 // Keypad +
#define KEY_KPENTER 0x58 // Keypad ENTER
#define KEY_KP1 0x59 // Keypad 1 and End
#define KEY_KP2 0x5a // Keypad 2 and Down Arrow
#define KEY_KP3 0x5b // Keypad 3 and PageDn
#define KEY_KP4 0x5c // Keypad 4 and Left Arrow
#define KEY_KP5 0x5d // Keypad 5
#define KEY_KP6 0x5e // Keypad 6 and Right Arrow
#define KEY_KP7 0x5f // Keypad 7 and Home
#define KEY_KP8 0x60 // Keypad 8 and Up Arrow
#define KEY_KP9 0x61 // Keypad 9 and Page Up
#define KEY_KP0 0x62 // Keypad 0 and Insert
#define KEY_KPDOT 0x63 // Keypad . and Delete

#define KEY_102ND 0x64 // Keyboard Non-US \ and |
#define KEY_COMPOSE 0x65 // Keyboard Application
#define KEY_POWER 0x66 // Keyboard Power
#define KEY_KPEQUAL 0x67 // Keypad =

//#define KEY_F13 0x68 // Keyboard F13
//#define KEY_F14 0x69 // Keyboard F14
//#define KEY_F15 0x6a // Keyboard F15
//#define KEY_F16 0x6b // Keyboard F16
//#define KEY_F17 0x6c // Keyboard F17
//#define KEY_F18 0x6d // Keyboard F18
//#define KEY_F19 0x6e // Keyboard F19
//#define KEY_F20 0x6f // Keyboard F20
//#define KEY_F21 0x70 // Keyboard F21
//#define KEY_F22 0x71 // Keyboard F22
//#define KEY_F23 0x72 // Keyboard F23
//#define KEY_F24 0x73 // Keyboard F24

#define KEY_OPEN 0x74 // Keyboard Execute
#define KEY_HELP 0x75 // Keyboard Help
#define KEY_PROPS 0x76 // Keyboard Menu
#define KEY_FRONT 0x77 // Keyboard Select
#define KEY_STOP 0x78 // Keyboard Stop
#define KEY_AGAIN 0x79 // Keyboard Again
#define KEY_UNDO 0x7a // Keyboard Undo
#define KEY_CUT 0x7b // Keyboard Cut
#define KEY_COPY 0x7c // Keyboard Copy
#define KEY_PASTE 0x7d // Keyboard Paste
#define KEY_FIND 0x7e // Keyboard Find
#define KEY_MUTE 0x7f // Keyboard Mute
#define KEY_VOLUMEUP 0x80 // Keyboard Volume Up
#define KEY_VOLUMEDOWN 0x81 // Keyboard Volume Down
// 0x82  Keyboard Locking Caps Lock
// 0x83  Keyboard Locking Num Lock
// 0x84  Keyboard Locking Scroll Lock
#define KEY_KPCOMMA 0x85 // Keypad Comma
// 0x86  Keypad Equal Sign
#define KEY_RO 0x87 // Keyboard International1
#define KEY_KATAKANAHIRAGANA 0x88 // Keyboard International2
#define KEY_YEN 0x89 // Keyboard International3
#define KEY_HENKAN 0x8a // Keyboard International4
#define KEY_MUHENKAN 0x8b // Keyboard International5
#define KEY_KPJPCOMMA 0x8c // Keyboard International6
// 0x8d  Keyboard International7
// 0x8e  Keyboard International8
// 0x8f  Keyboard International9
#define KEY_HANGEUL 0x90 // Keyboard LANG1
#define KEY_HANJA 0x91 // Keyboard LANG2
#define KEY_KATAKANA 0x92 // Keyboard LANG3
#define KEY_HIRAGANA 0x93 // Keyboard LANG4
#define KEY_ZENKAKUHANKAKU 0x94 // Keyboard LANG5
// 0x95  Keyboard LANG6
// 0x96  Keyboard LANG7
// 0x97  Keyboard LANG8
// 0x98  Keyboard LANG9
// 0x99  Keyboard Alternate Erase
// 0x9a  Keyboard SysReq/Attention
// 0x9b  Keyboard Cancel
// 0x9c  Keyboard Clear
// 0x9d  Keyboard Prior
// 0x9e  Keyboard Return
// 0x9f  Keyboard Separator
// 0xa0  Keyboard Out
// 0xa1  Keyboard Oper
// 0xa2  Keyboard Clear/Again
// 0xa3  Keyboard CrSel/Props
// 0xa4  Keyboard ExSel

// 0xb0  Keypad 00
// 0xb1  Keypad 000
// 0xb2  Thousands Separator
// 0xb3  Decimal Separator
// 0xb4  Currency Unit
// 0xb5  Currency Sub-unit
#define KEY_KPLEFTPAREN 0xb6 // Keypad (
#define KEY_KPRIGHTPAREN 0xb7 // Keypad )
// 0xb8  Keypad {
// 0xb9  Keypad }
// 0xba  Keypad Tab
// 0xbb  Keypad Backspace
// 0xbc  Keypad A
// 0xbd  Keypad B
// 0xbe  Keypad C
// 0xbf  Keypad D
// 0xc0  Keypad E
// 0xc1  Keypad F
// 0xc2  Keypad XOR
// 0xc3  Keypad ^
// 0xc4  Keypad %
// 0xc5  Keypad <
// 0xc6  Keypad >
// 0xc7  Keypad &
// 0xc8  Keypad &&
// 0xc9  Keypad |
// 0xca  Keypad ||
// 0xcb  Keypad :
// 0xcc  Keypad #
// 0xcd  Keypad Space
// 0xce  Keypad @
// 0xcf  Keypad !
// 0xd0  Keypad Memory Store
// 0xd1  Keypad Memory Recall
// 0xd2  Keypad Memory Clear
// 0xd3  Keypad Memory Add
// 0xd4  Keypad Memory Subtract
// 0xd5  Keypad Memory Multiply
// 0xd6  Keypad Memory Divide
// 0xd7  Keypad +/-
// 0xd8  Keypad Clear
// 0xd9  Keypad Clear Entry
// 0xda  Keypad Binary
// 0xdb  Keypad Octal
// 0xdc  Keypad Decimal
// 0xdd  Keypad Hexadecimal


#define KEY_LEFTCTRL 0xe0 // Keyboard Left Control
#define KEY_LEFTSHIFT 0xe1 // Keyboard Left Shift
#define KEY_LEFTALT 0xe2 // Keyboard Left Alt
#define KEY_LEFTMETA 0xe3 // Keyboard Left GUI
#define KEY_RIGHTCTRL 0xe4 // Keyboard Right Control
#define KEY_RIGHTSHIFT 0xe5 // Keyboard Right Shift
#define KEY_RIGHTALT 0xe6 // Keyboard Right Alt
#define KEY_RIGHTMETA 0xe7 // Keyboard Right GUI
/**
 * Modifier masks - used for the first byte in the HID report.
 * NOTE: The second byte in the report is reserved, 0x00
 */
#define KEY_MOD_LCTRL  0x01
#define KEY_MOD_LSHIFT 0x02
#define KEY_MOD_LALT   0x04
#define KEY_MOD_LMETA  0x08
#define KEY_MOD_RCTRL  0x10
#define KEY_MOD_RSHIFT 0x20
#define KEY_MOD_RALT   0x40
#define KEY_MOD_RMETA  0x80

#endif // USB_HID_KEYS

#ifndef _HID_MEDIA_KEYS
#define _HID_MEDIA_KEYS

// Media key bitflags
#define KEY_MEDIA_PLAY 0x1
#define KEY_MEDIA_PAUSE 0x2
#define KEY_MEDIA_RECORD 0x4
#define KEY_MEDIA_FASTFORWARD 0x8
#define KEY_MEDIA_REWIND 0x10
#define KEY_MEDIA_NEXTTRACK 0x20
#define KEY_MEDIA_PREVIOUSTRACK 0x40
#define KEY_MEDIA_STOP 0x80
#define KEY_MEDIA_EJECT 0x100
#define KEY_MEDIA_RANDOMPLAY 0x200
#define KEY_MEDIA_REPEAT 0x400
#define KEY_MEDIA_PLAYPAUSE 0x800
#define KEY_MEDIA_MUTE 0x1000
#define KEY_MEDIA_VOLUMEUP 0x2000
#define KEY_MEDIA_VOLUMEDOWN 0x4000
#define KEY_MEDIA_WWWHOME 0x8000
#define KEY_MEDIA_MYCOMPUTER 0x10000
#define KEY_MEDIA_CALCULATOR 0x20000
#define KEY_MEDIA_WWWFAVORITES 0x40000
#define KEY_MEDIA_WWWSEARCH 0x80000
#define KEY_MEDIA_WWWSTOP 0x100000
#define KEY_MEDIA_WWWBACK 0x200000
#define KEY_MEDIA_MEDIASELECT 0x400000
#define KEY_MEDIA_MAIL 0x800000
#endif // _HID_MEDIA_KEYS

#ifndef _HID_LED_KEYS
#define _HID_LED_KEYS
// LED bitflags
#define KEY_LED_NUMLOCK 0x1
#define KEY_LED_CAPSLOCK 0x2
#define KEY_LED_SCROLLLOCK 0x4
#define KEY_LED_COMPOSE 0x8
#define KEY_LED_KANA 0x10
#endif // _HID_LED_KEYS
