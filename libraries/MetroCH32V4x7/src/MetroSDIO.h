#pragma once
#include <Arduino.h>
#include <SdFat.h>

/** Native four-bit SDIO on the Metro's microSD socket.
 * Polling with hardware flow control. No formatting is performed. */
class MetroSDIO : public FsBlockDeviceInterface {
public:
  bool begin(bool fourBit = true);
  void end() override;
  bool isBusy() override;
  bool readSector(Sector_t sector, uint8_t *data) override;
  bool readSectors(Sector_t sector, uint8_t *data, size_t count) override;
  bool writeSector(Sector_t sector, const uint8_t *data) override;
  bool writeSectors(Sector_t sector, const uint8_t *data, size_t count) override;
  Sector_t sectorCount() override { return _ready ? _sectors : 0; }
  bool syncDevice() override;
  uint32_t error() const { return _error; }
  uint8_t lastCommand() const { return _command; }
  uint32_t response() const;
  uint32_t clockHz() const { return _clockHz; }
  uint32_t cid[4] = {};

private:
  bool command(uint8_t index, uint32_t argument, uint8_t responseType = 1, bool crc = true);
  bool cardReady();
  bool transfer(Sector_t sector, uint8_t *data, bool write);
  uint32_t _sectors = 0;
  uint32_t _error = 0;
  uint32_t _clockHz = 0;
  uint16_t _rca = 0;
  uint8_t _command = 0;
  bool _highCapacity = false;
  bool _ready = false;
};
