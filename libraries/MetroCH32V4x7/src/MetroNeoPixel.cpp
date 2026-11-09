#include "MetroNeoPixel.h"
#include "internal/ch32rv_gpio.h"
#include "internal/ch32rv_irq.h"
#include "internal/ch32rv_registers.h"

void metroNeoPixelWrite(uint8_t red, uint8_t green, uint8_t blue) {
  digitalWrite(PIN_NEOPIXEL, LOW);
  pinMode(PIN_NEOPIXEL, OUTPUT);
  delayMicroseconds(300);
  const uint8_t colors[] = {green, red, blue};
  const uint32_t period = F_CPU / 800000;
  const uint32_t zeroHigh = F_CPU / 2500000; // 400 ns
  const uint32_t oneHigh = F_CPU / 1250000;  // 800 ns
  uint32_t irq = ch32rv_irq_save();
  for (uint8_t value : colors) {
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      uint32_t highTicks = value & mask ? oneHigh : zeroHigh;
      CH32RV_GPIO_BSHR(2) = 1u << 14;
      uint32_t start = CH32RV_SYSTICK_CNT;
      while ((uint32_t)(CH32RV_SYSTICK_CNT - start) < highTicks) {
      }
      CH32RV_GPIO_BCR(2) = 1u << 14;
      while ((uint32_t)(CH32RV_SYSTICK_CNT - start) < period) {
      }
    }
  }
  ch32rv_irq_restore(irq);
}
