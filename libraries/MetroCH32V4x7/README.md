# Adafruit Metro CH32V407 / CH32V467

These variants target **Metro CH32V407 rev A**, with a separate Ethernet
transformer and RJ45. The same PCB may be fitted with CH32V467RET6. The older
PCB named Metro CH32V467 has a different pin map and is not covered.

## Arduino pin map

| Name | MCU pad | Notes |
|---|---|---|
| D0 / RX | PB7 | USART1 RX, remapped |
| D1 / TX | PB6 | USART1 TX, remapped |
| D2 | PB8 | |
| D3 | PB9 | PWM |
| D4 | PC0 | |
| D5 | PC1 | |
| D6 | PC2 | |
| D7 | PC3 | |
| D8 | PA0 | PWM |
| D9 | PA1 | PWM |
| D10 / SS | PA2 | PWM |
| D11 | PA8 | PWM; not MOSI |
| D12 | PA9 | PWM; not MISO |
| D13 / LED_BUILTIN | PB12 | Onboard LED; not SCK |
| A0 | PA4 | ADC4, DAC1 |
| A1 | PA3 | ADC3 |
| A2 | PB0 | ADC8 |
| A3 | PB1 | ADC9 |
| A4 | PC4 | ADC14 |
| A5 | PC5 | ADC15 |
| SDA / SCL | PB11 / PB10 | Header and STEMMA QT |
| MOSI / MISO / SCK | PA7 / PA6 / PA5 | Separate SPI header |
| PIN_NEOPIXEL | PC14 | One GRB pixel |
| PIN_BUTTON | PC15 | Active low |

Numbers 0-13 select D0-D13; 14-19 select A0-A5; 20/21 select SDA/SCL.
MCU pad constants keep the core's encoded values. `Serial` is D0/D1 UART;
`SerialUSB` is native USB CDC. The two buses are distinct.

`Wire` uses the existing **SoftWire controller** on PB11/PB10. Those pads are
connected to the silicon's I3C peripheral, which the core's conventional I2C
driver cannot use. I3C hardware and I2C target mode are not implemented here.

The SDIO pins are PC12 CLK, PD2 CMD, PB14 D0, PB15 D1, PC10 D2, PC11 D3;
card detect is PA10. External flash uses PA15 CS and PB3/PB4/PB5 SCK/MISO/MOSI.
PD8 on the 407, or VDD18 on the 467, is connected to supply capacitors and must
not be driven as a GPIO.

## Clock, memory and upload

Both variants use the 25 MHz crystal with SYSCLK and HCLK at 200 MHz. Their
HSI fallback also selects 200 MHz. TinyUF2 occupies the first 32 KiB;
applications link at `0x8000`, with 480 KiB flash and 200 KiB SRAM available.
This does not change option bytes or the chip's flash/RAM split.

USB IDs are Adafruit 239A:817D (407) and 239A:817F (467). Opening and closing
`SerialUSB` at 1200 baud enters TinyUF2. Double RESET also works. A probe upload
uses sector erase and the application's ELF addresses, preserving TinyUF2.
The Arduino IDE Upload command still uses the core's probe uploader; an
integrated TinyUF2 uploader/package release is not supplied by this change.

For USB-only uploads, convert the exported binary with the existing
[UF2 converter](https://github.com/microsoft/uf2/blob/master/utils/uf2conv.py):

```sh
python uf2conv.py -c -f 0x7b8c32a7 -b 0x8000 -o sketch.uf2 sketch.bin
```

Enter TinyUF2 and copy `sketch.uf2` to METRO407 or METRO467. The family ID is
[registered as CH32V4X7](https://github.com/microsoft/uf2/pull/109). The bootloader port is
[adafruit/tinyuf2#639](https://github.com/adafruit/tinyuf2/pull/639).

## Board helpers

- `MetroPSRAM`: initialize the 467RET6's 4 MiB in-package PSRAM and expose it
  through `data()` and `size()`. It is not added to `malloc`/`new`. The 407 has
  no PSRAM; `begin()` returns false there. The initializer checks the fitted
  chip and requires the 200 MHz system-clock profile.
- `MetroSDIO`: native four-bit SDIO block device, compatible with SdFat's
  `FsVolume`, FAT and exFAT. Uses polling and hardware flow control at 10 MHz,
  bounded waits, card-status checks and completed block CRC. Multi-sector
  requests use single-block commands. It never formats a card.
- `metroNeoPixelWrite(red, green, blue)`: drive the single onboard pixel.
  Interrupts pause for approximately 30 us. This is not an Adafruit_NeoPixel
  library port or a general strip driver.

Install **SdFat - Adafruit Fork**. Sketches using `FsVolume` with `MetroSDIO`
need `-DUSE_BLOCK_DEVICE_INTERFACE=1` in their `build_opt.h`, as in MetroBringup.
The bench fixture in `extras/MetroBringup/MetroBringup.ino` additionally uses **Sensirion I2C STC42** and its Core
dependency. Open that sketch directly in the IDE after installing these
libraries. It lives in `extras` because the automated bundled-example builds
do not install external fixture dependencies. USB, Wire, SPI, timer and Network
libraries come with this core.

## Hardware verification, 2026-11-09

MetroBringup starts without an open serial monitor. Its jumper fixture is
D0-D1, D2-D3, D4-D5, D6-D7, D8-D9, D10-D11, D12-D13, A0-A1, A2-A3, A4-A5,
and MOSI-MISO on the separate SPI header. Install a microSD card, connect an
STC42 at 0x2B, and connect Ethernet to DHCP. Do not jumper power pins.

On a physical CH32V467RET6, the following passed:

- All ten GPIO pairs in both directions and 100 external-interrupt edges.
- UART loopback of all 256 byte values; SPI loopback in modes 0-3.
- All six ADC inputs near 0/4095 at the supply rails. This is a functional
  check, not an ADC accuracy/calibration measurement.
- TIM2 PWM measured by TIM1 capture: 1000 Hz and 25% duty. TIM6 produced 99
  interrupts during a 100 ms interval at its requested 1 kHz rate.
- STC42 product information and three measurements with valid CRCs.
- Full 4 MiB PSRAM address-dependent patterns and complements, plus byte and
  halfword writes over 4 KiB. Sub-word support is verified on this chip only;
  older PSRAM lots may have restrictions.
- Four-bit SDIO at 10 MHz: repeated reads, FAT32 mount, and a new 32 KiB file
  written, closed, reopened and checked byte-for-byte. Existing files remain.
- Ethernet: 100 Mbps full duplex, DHCP, DNS, TCP and HTTP 200, zero receive errors.
- USB echo at 63/64/65, 511/512/513, 4096 and 16384 bytes while Ethernet and
  the NeoPixel were active; 1200-baud entry and UF2 reload without a probe.
- D13 blink and red/green/blue NeoPixel colors observed on the board.

Both Arduino variants compile. The full peripheral fixture has **not yet run
on a 407**; its earlier TinyUF2/blink/USB hardware checks passed. Button presses,
external flash, exFAT cards, hot removal, SDIO DMA/high-speed modes and native
I3C remain untested or unimplemented.

Send `t` over USB to repeat the read-only run; `e` repeats Ethernet. Send `w`
once to create and verify a new `METROnn.TST` file (32 KiB, exclusive creation).
Test files are kept on the card. Startup and `t` do not write files.
