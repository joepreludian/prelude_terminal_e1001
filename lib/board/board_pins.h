#pragma once

// reTerminal E1001 pin map. Single source of truth.
// Verified against wiki.seeedstudio.com/reterminal_e10xx_with_arduino_peripherals/

constexpr int PIN_BTN_GREEN = 3;   // active low, hardware pull-up
constexpr int PIN_BTN_RIGHT = 4;   // active low
constexpr int PIN_BTN_LEFT  = 5;   // active low

constexpr int PIN_LED       = 6;   // inverted: LOW = on
constexpr int PIN_BUZZER    = 45;  // passive buzzer, drive with tone()

constexpr int PIN_BATT_ADC  = 1;   // voltage = analogReadMilliVolts * 2
constexpr int PIN_BATT_EN   = 21;  // HIGH enables the divider

constexpr int PIN_I2C_SDA   = 19;  // SHT4x (0x44), PCF8563 (0x51)
constexpr int PIN_I2C_SCL   = 20;
