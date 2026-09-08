#pragma once
#include <Arduino.h>

// The reTerminal E1001 routes its USB-C port through a CH340 bridge wired to
// UART0 (GPIO 43/44), not to the ESP32-S3's native USB. With USB CDC enabled
// by the board definition, `Serial` is the native USB port and `Serial0` is
// UART0, so all logging goes to Serial0.
#define LOG_SERIAL Serial0

#define LOG(fmt, ...) \
  LOG_SERIAL.printf("[%8lu] " fmt "\n", (unsigned long)millis(), ##__VA_ARGS__)
