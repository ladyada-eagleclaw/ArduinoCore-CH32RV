/* Native SDIO: WCH CH32V407 RM v1.2 chapter 24, and the SD Physical Layer
 * command sequence. Metro uses the remap to PB14/PB15/PC10/PC11/PC12/PD2. */
#include "MetroSDIO.h"
#include "internal/ch32rv_gpio.h"
#include "internal/ch32rv_registers.h"
#include <string.h>

namespace {
struct SdioRegisters {
  volatile uint32_t power, clock, argument, command, responseCommand;
  volatile uint32_t response[4];
  volatile uint32_t timer, length, control, count, status, clear, mask;
  uint32_t reserved0[2];
  volatile uint32_t fifoCount;
  uint32_t reserved1[5];
  volatile uint32_t control2;
  uint32_t reserved2[7];
  volatile uint32_t fifo;
};
SdioRegisters &sd = *(SdioRegisters *)0x40018000u;
constexpr uint32_t commandCrcError = 1;
constexpr uint32_t commandTimeout = 4;
constexpr uint32_t responseEnd = 0x40;
constexpr uint32_t commandSent = 0x80;
constexpr uint32_t blockCrcEnd = 0x400;
constexpr uint32_t dataErrors = 0x23a;
constexpr uint32_t receiveAvailable = 0x200000;
constexpr uint32_t transmitHalfEmpty = 0x4000;
constexpr uint32_t clockEnable = 0x100;
constexpr uint32_t fourBitBus = 0x800;
constexpr uint32_t fallingEdge = 0x2000;
constexpr uint32_t flowControl = 0x4000;
constexpr uint32_t dataRead512 = 0x93;
constexpr uint32_t dataWrite512 = 0x91;
constexpr uint32_t softwareTimeout = 0x80000000;
constexpr uint32_t cardErrors = 0xfdffe008;
} // namespace

uint32_t MetroSDIO::response() const { return sd.response[0]; }

bool MetroSDIO::command(uint8_t index, uint32_t argument, uint8_t responseType, bool crc) {
  _command = index;
  sd.clear = commandCrcError | commandTimeout | responseEnd | commandSent;
  sd.argument = argument;
  sd.command = index | ((uint32_t)responseType << 6) | 0x400;
  uint32_t start = millis();
  uint32_t status;
  do {
    status = sd.status;
    if (millis() - start > 100) {
      _error = softwareTimeout;
      return false;
    }
  } while (!(status & (commandCrcError | commandTimeout | responseEnd | commandSent)));
  _error = status & (commandTimeout | (crc ? commandCrcError : 0));
  return _error == 0;
}

bool MetroSDIO::begin(bool fourBit) {
  _ready = false;
  _sectors = 0;
  _error = 0;
  pinMode(PIN_SD_DETECT, INPUT_PULLUP);
  // SDIO and ETH_MAC clocks are both enabled in WCH's SDIO example.
  CH32RV_RCC_AHBPCENR |= 0x10400;
  ch32rv_clock_enable(AFIO);
  CH32RV_AFIO_PCFR2 |= 0x1000;
  const uint8_t pins[] = {PIN_SD_D0, PIN_SD_D1, PIN_SD_D2, PIN_SD_D3, PIN_SD_CLK, PIN_SD_CMD};
  for (uint8_t pin : pins) {
    ch32rv_gpio_clock_enable(CH32RV_PIN_PORT(pin));
    ch32rv_gpio_set_config(CH32RV_PIN_PORT(pin), CH32RV_PIN_BIT(pin), 0xb);
  }
  sd.power = 0;
  sd.clock = 0;
  sd.command = 0;
  sd.control = 0;
  sd.mask = 0;
  sd.clear = 0xc007ff;
  // Peripheral input is HCLK/2. Identification must not exceed 400 kHz.
  uint32_t divisor = (F_CPU / 2 + 399999) / 400000;
  if (divisor < 2 || divisor > 257)
    return false;
  sd.clock = (divisor - 2) | fallingEdge | clockEnable;
  sd.power = 3;
  delay(5); // At least 74 idle clocks before CMD0.
  if (!command(0, 0, 0))
    return false;
  bool version2 = command(8, 0x1aa);
  if (version2 && (response() & 0xfff) != 0x1aa)
    return false;
  uint32_t start = millis();
  uint32_t ocr;
  do {
    if (!command(55, 0))
      return false;
    if (!command(41, version2 ? 0x40ff8000 : 0x00ff8000, 1, false))
      return false;
    ocr = response();
    if (millis() - start > 1500) {
      _error = softwareTimeout;
      return false;
    }
    delay(1);
  } while (!(ocr & 0x80000000));
  _highCapacity = ocr & 0x40000000;
  if (!command(2, 0, 3))
    return false;
  for (unsigned i = 0; i < 4; i++)
    cid[i] = sd.response[i];
  if (!command(3, 0))
    return false;
  _rca = response() >> 16;
  if (!_rca || !command(9, (uint32_t)_rca << 16, 3))
    return false;
  uint32_t csd[4];
  for (unsigned i = 0; i < 4; i++)
    csd[i] = sd.response[i];
  if ((csd[0] >> 30) == 1) {
    uint32_t size = ((csd[1] & 0x3f) << 16) | (csd[2] >> 16);
    _sectors = (size + 1) * 1024;
  } else {
    uint32_t blockBits = (csd[1] >> 16) & 15;
    uint32_t size = ((csd[1] & 0x3ff) << 2) | (csd[2] >> 30);
    uint32_t multiplier = (csd[2] >> 15) & 7;
    _sectors = (((uint64_t)size + 1) << (multiplier + 2 + blockBits)) / 512;
  }
  if (!command(7, (uint32_t)_rca << 16) || !cardReady())
    return false;
  if (!_highCapacity && !command(16, 512))
    return false;
  if (fourBit) {
    if (!command(55, (uint32_t)_rca << 16) || !command(6, 2))
      return false;
    if (response() & cardErrors)
      return false;
  }
  // Start at 10 MHz, with flow control allowing USB interrupts during polling.
  divisor = F_CPU / 2 / 10000000;
  sd.clock = (divisor - 2) | clockEnable | fallingEdge | flowControl | (fourBit ? fourBitBus : 0);
  _clockHz = F_CPU / 2 / divisor;
  _ready = true;
  return true;
}

bool MetroSDIO::cardReady() {
  uint32_t start = millis();
  do {
    if (!command(13, (uint32_t)_rca << 16))
      return false;
    uint32_t state = response();
    if (state & cardErrors) {
      _error = state;
      return false;
    }
    if ((state & 0x1f00) == 0x900)
      return true; // READY_FOR_DATA, TRAN state.
  } while (millis() - start < 500);
  _error = softwareTimeout;
  return false;
}

bool MetroSDIO::transfer(Sector_t sector, uint8_t *data, bool write) {
  if (!_ready || sector >= _sectors || !cardReady())
    return false;
  sd.control = 0;
  sd.clear = 0x7ff;
  sd.timer = _clockHz / 2;
  sd.length = 512;
  if (!write)
    sd.control = dataRead512;
  uint32_t address = _highCapacity ? sector : sector * 512;
  if (!command(write ? 24 : 17, address)) {
    sd.control = 0;
    return false;
  }
  if (response() & cardErrors) {
    _error = response();
    sd.control = 0;
    return false;
  }
  if (write)
    sd.control = dataWrite512;
  unsigned words = 0;
  uint32_t start = millis();
  while (true) {
    uint32_t status = sd.status;
    if (status & dataErrors) {
      _error = status;
      break;
    }
    if (!write) {
      while ((sd.status & receiveAvailable) && words < 128) {
        uint32_t value = sd.fifo;
        memcpy(data + 4 * words++, &value, 4);
      }
    } else if ((status & transmitHalfEmpty) && words < 128) {
      for (unsigned n = 0; n < 8 && words < 128; n++) {
        uint32_t value;
        memcpy(&value, data + 4 * words++, 4);
        sd.fifo = value;
      }
    }
    if ((sd.status & blockCrcEnd) && words == 128) {
      // DATAEND alone only means the byte counter reached zero, before CRC.
      if (sd.status & dataErrors) {
        _error = sd.status;
        break;
      }
      sd.control = 0;
      sd.clear = 0x7ff;
      return !write || cardReady();
    }
    if (millis() - start > 600) {
      _error = softwareTimeout;
      break;
    }
  }
  sd.control = 0;
  sd.clear = 0x7ff;
  return false;
}

bool MetroSDIO::readSector(Sector_t sector, uint8_t *data) { return transfer(sector, data, false); }
bool MetroSDIO::writeSector(Sector_t sector, const uint8_t *data) {
  return transfer(sector, (uint8_t *)data, true);
}
bool MetroSDIO::readSectors(Sector_t sector, uint8_t *data, size_t count) {
  if (sector > _sectors || count > _sectors - sector)
    return false;
  for (size_t i = 0; i < count; i++) {
    if (!readSector(sector + i, data + 512 * i))
      return false;
  }
  return true;
}
bool MetroSDIO::writeSectors(Sector_t sector, const uint8_t *data, size_t count) {
  if (sector > _sectors || count > _sectors - sector)
    return false;
  for (size_t i = 0; i < count; i++) {
    if (!writeSector(sector + i, data + 512 * i))
      return false;
  }
  return true;
}
bool MetroSDIO::syncDevice() { return _ready && cardReady(); }
bool MetroSDIO::isBusy() {
  if (!_ready || !command(13, (uint32_t)_rca << 16))
    return true;
  return (response() & 0x1f00) != 0x900;
}
void MetroSDIO::end() {
  _ready = false;
  sd.control = 0;
  sd.clock = 0;
  sd.power = 0;
  _sectors = 0;
}
