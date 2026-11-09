/* PSRAM setup follows WCH CH32V4x7 EVT PSRAM example at
 * afee2d144b184176e5ca21202714ca9add1dd46f. Register operations deliberately
 * mirror the controller's command/busy sequencing, with bounded waits.
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * WCH software, modified or not, is for WCH microcontrollers only. */
#include "MetroPSRAM.h"
#include "internal/ch32rv_registers.h"

namespace {
struct PsramRegisters {
  volatile uint32_t control, timing, data, address, command0, command1;
  volatile uint32_t dmaControl, count, memoryAddress, psramAddress;
  volatile uint32_t status, interruptEnable, state, ceConfig, latency;
};
PsramRegisters &ram = *(PsramRegisters *)0x40024400u;
constexpr uint32_t ramClock = 0x10;
constexpr uint32_t commandBusy = 1;
constexpr uint32_t commandWrite = 2;
constexpr uint32_t capacity32Mbit = 0x100;
constexpr uint32_t exitLowPower = 0x10000;
} // namespace

bool MetroPSRAM::waitReady() {
  uint32_t start = micros();
  while (ram.address & commandBusy) {
    if (micros() - start >= 10000)
      return false;
  }
  return true;
}

bool MetroPSRAM::writeModeRegister(uint8_t address, uint16_t value) {
  if (!waitReady())
    return false;
  ram.data = value;
  uint32_t command = (ram.command1 >> 8) & 0xff;
  ram.address = ((uint32_t)address << 16) | (command << 8);
  ram.address |= commandBusy;
  ram.address |= commandWrite;
  return waitReady();
}

int MetroPSRAM::readModeRegister(uint8_t address) {
  if (!_ready || address > 15 || !waitReady())
    return -1;
  uint32_t command = ram.command1 & 0xff;
  ram.address = ((uint32_t)address << 16) | (command << 8);
  ram.address |= commandBusy;
  ram.address &= ~commandWrite;
  if (!waitReady())
    return -1;
  return ram.data & 0xffff;
}

bool MetroPSRAM::begin() {
  _ready = false;
#if defined(METRO_CH32V467) && F_CPU == 200000000L && CH32RV_CLOCK_SYSCLK_HZ == 200000000
  const uint32_t chip = CH32RV_REG32(0x1ffff704u);
  if ((chip & 0xffff000fu) != 0x46750002u)
    return false;
  // WCH's 200 MHz profile uses SYSCLK=HCLK=200 MHz, not SYSCLK=400 MHz.
  CH32RV_RCC_AHBPCENR |= ramClock;
  CH32RV_REG32(0x40021028u) |= ramClock;
  CH32RV_REG32(0x40021028u) &= ~ramClock;
  ram.control = capacity32Mbit | exitLowPower;
  // trc=20, tcph=12, txlpd=7, as in EVT's 200 MHz PSRAM example.
  ram.timing = 20u | (12u << 16) | (7u << 24);
  delay(1);
  ram.latency = 7u << 8;
  if (!writeModeRegister(4, 1u << 5))
    return false;
  ram.latency |= 7;
  if (!writeModeRegister(0, 4u << 2))
    return false;
  _ready = true;
  return true;
#else
  return false;
#endif
}
