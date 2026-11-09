/* Shared PCB: Metro CH32V407 rev A, also fitted with CH32V467RET6.
 * Verified from schematic/board nets, not the older CH32V467 PCB. */
#pragma once
#define LED_BUILTIN PB12
#define PIN_LED PB12
#define PIN_SERIAL_TX PB6
#define PIN_SERIAL_RX PB7
#define PIN_WIRE_SCL PB10
#define PIN_WIRE_SDA PB11
#define SCL PIN_WIRE_SCL
#define SDA PIN_WIRE_SDA
#define PIN_SPI_SCK PA5
#define PIN_SPI_MISO PA6
#define PIN_SPI_MOSI PA7
#define PIN_SPI_SS PA2
#define SCK PIN_SPI_SCK
#define MISO PIN_SPI_MISO
#define MOSI PIN_SPI_MOSI
#define SS PIN_SPI_SS
#ifdef METRO_CH32V467
#include "../CH32V467/pins_arduino.h"
#else
#include "../CH32V407/pins_arduino.h"
#endif
#define CH32RV_BOARD_PIN_COUNT 22
#define CH32RV_BOARD_PINS {PB7, PB6, PB8, PB9, PC0, PC1, PC2, PC3, PA0, PA1, PA2, PA8, PA9, PB12, PA4, PA3, PB0, PB1, PC4, PC5, PB11, PB10}
#undef NUM_DIGITAL_PINS
#undef PINS_COUNT
#define NUM_DIGITAL_PINS CH32RV_BOARD_PIN_COUNT
#define PINS_COUNT NUM_DIGITAL_PINS
#define D0 PB7
#define D1 PB6
#define D2 PB8
#define D3 PB9
#define D4 PC0
#define D5 PC1
#define D6 PC2
#define D7 PC3
#define D8 PA0
#define D9 PA1
#define D10 PA2
#define D11 PA8
#define D12 PA9
#define D13 PB12
#undef A0
#undef A1
#undef A2
#undef A3
#undef A4
#undef A5
#undef A6
#undef A7
#undef A8
#undef A9
#undef A10
#undef A11
#undef A12
#undef A13
#undef A14
#undef A15
#undef NUM_ANALOG_INPUTS
#define A0 PA4
#define A1 PA3
#define A2 PB0
#define A3 PB1
#define A4 PC4
#define A5 PC5
#define NUM_ANALOG_INPUTS 6
#define PIN_NEOPIXEL PC14
#define NEOPIXEL_NUM 1
#define PIN_BUTTON PC15
#define PIN_VBUS_SENSE PB13
#define PIN_FLASH_CS PA15
#define PIN_FLASH_SCK PB3
#define PIN_FLASH_MISO PB4
#define PIN_FLASH_MOSI PB5
#define PIN_SD_DETECT PA10
#define PIN_SD_CLK PC12
#define PIN_SD_CMD PD2
#define PIN_SD_D0 PB14
#define PIN_SD_D1 PB15
#define PIN_SD_D2 PC10
#define PIN_SD_D3 PC11
#define PIN_ETH_LED0 PD14
#define PIN_ETH_LED1 PD15
/* PD8 (407) / VDD18 (467) is connected to capacitors. Never use as GPIO. */

/* Wire defaults to the actual header/STEMMA pads through SoftWire. */
#undef CH32RV_I2C1_SDA
#undef CH32RV_I2C1_SCL
#define CH32RV_I2C1_SDA PB11
#define CH32RV_I2C1_SCL PB10
#define CH32RV_USB_DEVICE_VID 0x239a
#define CH32RV_USB_DEVICE_PID METRO_USB_PID
#define CH32RV_USB_DEVICE_MANUFACTURER "Adafruit"
#ifdef METRO_CH32V467
#define CH32RV_USB_DEVICE_PRODUCT "Metro CH32V467"
#else
#define CH32RV_USB_DEVICE_PRODUCT "Metro CH32V407"
#endif
#define CH32RV_USB_DEFAULT_DFU_RUNTIME false
#ifdef __cplusplus
extern "C" {
#endif
void metroPrepareBootloader(void);
#ifdef __cplusplus
}
#endif
#define CH32RV_USB_BOOTLOADER_ENTRY() metroPrepareBootloader()
