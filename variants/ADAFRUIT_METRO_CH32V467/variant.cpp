#include "internal/ch32rv_registers.h"
#include <Arduino.h>

void initVariant(void) {
  // D0/D1 are the remapped USART1 route.
  Serial.setPins(PB7, PB6);
}

extern "C" void metroPrepareBootloader(void) {
  // TinyUF2's 16-bit double-tap marker, in backup register 10.
  CH32RV_RCC_APB1PCENR |= (1u << 27) | (1u << 28);
  CH32RV_REG32(0x40007000u) |= 1u << 8;
  CH32RV_REG16(0x40006c28u) = 0xf016;
}
