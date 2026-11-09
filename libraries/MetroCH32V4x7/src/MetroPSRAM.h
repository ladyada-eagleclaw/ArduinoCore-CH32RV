#pragma once
#include <Arduino.h>

/** In-package 4 MiB PSRAM on CH32V467RET6, at a 200 MHz system clock.
 * CH32V407 has no PSRAM. Memory is not added to the ordinary heap.
 * Start with aligned 32-bit accesses; byte-write support depends on silicon. */
class MetroPSRAM {
public:
  bool begin();
  uint32_t size() const { return _ready ? 4 * 1024 * 1024 : 0; }
  volatile uint32_t *data() const { return _ready ? (volatile uint32_t *)0x80000000u : nullptr; }
  int readModeRegister(uint8_t address);

private:
  bool waitReady();
  bool writeModeRegister(uint8_t address, uint16_t value);
  bool _ready = false;
};
