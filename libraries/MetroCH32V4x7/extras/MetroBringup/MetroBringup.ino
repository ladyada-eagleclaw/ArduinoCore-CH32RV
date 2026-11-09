#include <Arduino.h>
#ifndef LED_BUILTIN
#error "Select an Adafruit Metro CH32V407 or CH32V467 board."
#endif
#include <CH32RVUsbDevice.h>
#include <CH32RVTimerInput.h>
#include <CH32RVTimer.h>
#include <SPI.h>
#include <Wire.h>
#include <Network.h>
#include <MetroNeoPixel.h>
#include <MetroPSRAM.h>
#include <MetroSDIO.h>
#include <SensirionI2cStc42.h>
#include "internal/ch32rv_registers.h"
#include "internal/ch32rv_gpio.h"

// Ten jumper pairs, plus MOSI-MISO on the separate SPI header.
const uint16_t pairs[][2] = {{0,1},{2,3},{4,5},{6,7},{8,9},{10,11},{12,13},
                            {A0,A1},{A2,A3},{A4,A5}};
TwoWire &stemma = Wire; // The Metro variant routes Wire to PB11/PB10.
volatile uint32_t edges = 0;
volatile uint32_t ticks = 0;
bool ran = false;
uint32_t lastBlink = 0;
bool led = false;

void setup() {
  SerialUSB.setBootTouchEnabled(true);
  CH32RVUsbDeviceConfig config;
  // Identity, 25 MHz crystal, and TinyUF2 entry come from the board variant.
  USBDevice.begin(config);
  SerialUSB.begin();
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  if (!ran && millis() > 6000) {
    ran = true;
    runTests();
  }
  while (SerialUSB.available()) {
    char command = SerialUSB.read();
    if (command == 't') {
      ran = true;
      runTests();
    } else if (command == 'w') {
      testSDFile();
    } else if (command == 'e') {
      testEthernet();
    } else {
      SerialUSB.write(command);
    }
  }
  if (millis() - lastBlink >= 500) {
    lastBlink = millis();
    led = !led;
    digitalWrite(LED_BUILTIN, led);
    static unsigned color = 0;
    metroNeoPixelWrite(color == 0 ? 12 : 0, color == 1 ? 12 : 0, color == 2 ? 12 : 0);
    color = (color + 1) % 3;
  }
}

void result(const char *name, bool pass) {
  SerialUSB.print(pass ? "PASS " : "FAIL ");
  SerialUSB.print(name);
  SerialUSB.print("\n\r");
  SerialUSB.flush();
}

void releaseHeaders() {
  for (unsigned i = 0; i < 10; i++) {
    pinMode(pairs[i][0], INPUT);
    pinMode(pairs[i][1], INPUT);
  }
}

void edge() { edges++; }
void timerTick(void *) { ticks++; }

void runTests() {
  SerialUSB.print("\n\rAdafruit Metro CH32V4x7 hardware test\n\r");
  SerialUSB.flush();
  releaseHeaders();
  for (unsigned i = 0; i < 10; i++) {
    bool pass = true;
    for (unsigned direction = 0; direction < 2; direction++) {
      uint16_t out = pairs[i][direction];
      uint16_t in = pairs[i][1-direction];
      pinMode(in, INPUT);
      digitalWrite(out, LOW);
      pinMode(out, OUTPUT);
      for (unsigned value = 0; value < 2; value++) {
        digitalWrite(out, value ? HIGH : LOW);
        delayMicroseconds(20);
        pass &= digitalRead(in) == (value ? HIGH : LOW);
      }
      pinMode(out, INPUT);
    }
    SerialUSB.print("Pair "); SerialUSB.print(i);
    SerialUSB.print(" "); result("GPIO both directions", pass);
  }

  pinMode(D3, INPUT);
  pinMode(D2, OUTPUT);
  digitalWrite(D2, LOW);
  edges = 0;
  attachInterrupt(digitalPinToInterrupt(D3), edge, RISING);
  for (unsigned i = 0; i < 100; i++) {
    digitalWrite(D2, HIGH); delayMicroseconds(100);
    digitalWrite(D2, LOW); delayMicroseconds(100);
  }
  delay(1); // Let the final pending edge run before detaching the callback.
  SerialUSB.print("IRQ edges="); SerialUSB.print(edges); SerialUSB.print("\n\r");
  detachInterrupt(digitalPinToInterrupt(D3));
  result("GPIO interrupt 100 edges", edges == 100);
  releaseHeaders();

  Serial.begin(115200);
  bool uartPass = true;
  while (Serial.available()) Serial.read();
  for (unsigned i = 0; i < 256; i++) {
    Serial.write((uint8_t)i);
    uint32_t start = millis();
    while (!Serial.available() && millis() - start < 20) {}
    if (Serial.read() != (int)i) uartPass = false;
  }
  result("UART D1 to D0, all 256 bytes", uartPass);
  Serial.end();
  releaseHeaders();

  analogReadResolution(12);
  for (unsigned i = 7; i < 10; i++) {
    bool pass = true;
    for (unsigned direction = 0; direction < 2; direction++) {
      uint16_t out = pairs[i][direction];
      uint16_t in = pairs[i][1-direction];
      pinMode(out, OUTPUT);
      digitalWrite(out, LOW); delay(2);
      int low = analogRead(in);
      digitalWrite(out, HIGH); delay(2);
      int high = analogRead(in);
      pass &= low < 120 && high > 3975;
      SerialUSB.print("ADC "); SerialUSB.print(in);
      SerialUSB.print(" low="); SerialUSB.print(low);
      SerialUSB.print(" high="); SerialUSB.print(high);
      SerialUSB.print("\n\r");
      pinMode(out, INPUT);
      pinMode(in, INPUT);
    }
    result("ADC paired rail readings", pass);
  }

  SPI.begin();
  for (unsigned mode = 0; mode < 4; mode++) {
    SPI.beginTransaction(SPISettings(1000000, MSBFIRST, mode));
    bool pass = true;
    for (unsigned i = 0; i < 256; i++) {
      if (SPI.transfer((uint8_t)i) != i) pass = false;
    }
    SPI.endTransaction();
    SerialUSB.print("SPI mode "); SerialUSB.print(mode);
    SerialUSB.print(" "); result("MOSI to MISO", pass);
  }
  SPI.end();

  CH32RVPulseCapture capture;
  bool pwmPass = capture.begin(D11, 100);
  analogWriteResolution(8);
  analogWriteFrequency(D10, 1000);
  analogWrite(D10, 64);
  CH32RVPulseCapture::Pulse pulse;
  unsigned samples = 0;
  uint32_t start = millis();
  while (millis() - start < 100) {
    if (capture.read(pulse)) samples++;
  }
  if (samples) {
    SerialUSB.print("PWM Hz="); SerialUSB.print(pulse.frequencyHz());
    SerialUSB.print(" duty="); SerialUSB.print(pulse.duty(), 3);
    SerialUSB.print("\n\r");
    pwmPass &= abs((int)pulse.frequencyHz() - 1000) < 10;
    pwmPass &= pulse.duty() > 0.23 && pulse.duty() < 0.27;
  } else pwmPass = false;
  result("PWM TIM2 to TIM1 capture", pwmPass);
  capture.end();
  analogWrite(D10, 0);
  releaseHeaders();

  CH32RVTimerOwner owner = {&ran, nullptr, nullptr};
  CH32RVTimerRequest request = {6, CH32RV_TIMER_WHOLE, {199, 999, 0}};
  CH32RVTimerLease lease = ch32rv_timer_try_acquire(&request, &owner);
  bool timerPass = ch32rv_timer_lease_valid(&lease);
  ticks = 0;
  if (timerPass) {
    ch32rv_timer_apply_base(&lease);
    ch32rv_timer_attach_update_interrupt(&lease, timerTick, nullptr);
    ch32rv_timer_start(&lease);
    delay(100);
    ch32rv_timer_stop(&lease);
    timerPass = ticks >= 98 && ticks <= 102;
    ch32rv_timer_release(&lease);
  }
  SerialUSB.print("Timer ticks="); SerialUSB.print(ticks); SerialUSB.print("\n\r");
  result("TIM6 interrupt at 1 kHz", timerPass);

  stemma.begin();
  unsigned found = 0;
  for (unsigned address = 8; address < 120; address++) {
    stemma.beginTransmission(address);
    if (!stemma.endTransmission()) {
      SerialUSB.print("I2C address 0x"); SerialUSB.print(address, HEX);
      SerialUSB.print("\n\r"); found++;
    }
  }
  result("STEMMA I2C responds (software controller)", found > 0);
  testSensor();
  stemma.end();
  testPSRAM();
  testSDIO();
  testEthernet();
  pinMode(LED_BUILTIN, OUTPUT);
  SerialUSB.print("Test run finished. Send t to repeat. D13 blinks continuously.\n\r");
}


void testPSRAM() {
#ifdef METRO_CH32V467
  MetroPSRAM psram;
  bool pass = psram.begin();
  result("PSRAM initialization", pass);
  if (!pass) return;
  SerialUSB.print("PSRAM MR0="); SerialUSB.print(psram.readModeRegister(0), HEX);
  SerialUSB.print(" MR1="); SerialUSB.print(psram.readModeRegister(1), HEX);
  SerialUSB.print(" MR2="); SerialUSB.print(psram.readModeRegister(2), HEX);
  SerialUSB.print(" MR4="); SerialUSB.print(psram.readModeRegister(4), HEX);
  SerialUSB.print("\n\r"); SerialUSB.flush();
  volatile uint32_t *memory = psram.data();
  uint32_t words = psram.size() / 4;
  uint32_t errors = 0;
  for (uint32_t i = 0; i < words; i++) memory[i] = i ^ 0x5a39c6e1;
  for (uint32_t i = 0; i < words; i++) {
    if (memory[i] != (i ^ 0x5a39c6e1)) errors++;
    memory[i] = ~(i ^ 0x5a39c6e1);
  }
  for (uint32_t i = 0; i < words; i++) {
    if (memory[i] != ~(i ^ 0x5a39c6e1)) errors++;
  }
  SerialUSB.print("PSRAM word errors="); SerialUSB.print(errors);
  SerialUSB.print("\n\r");
  result("PSRAM 4 MiB address patterns and inverse", errors == 0);
  // Verify sub-word writes do not alter their neighbours on this silicon lot.
  volatile uint8_t *bytes = (volatile uint8_t *)memory;
  volatile uint16_t *halves = (volatile uint16_t *)memory;
  for (unsigned i = 0; i < 1024; i++) memory[i] = 0xa55a3cc3;
  bool bytePass = true;
  for (unsigned i = 0; i < 4096; i++) bytes[i] = (i * 29 + 17) % 256;
  for (unsigned i = 0; i < 4096; i++) {
    if (bytes[i] != (i * 29 + 17) % 256) bytePass = false;
  }
  result("PSRAM byte writes across 4 KiB", bytePass);
  bool halfPass = true;
  for (unsigned i = 0; i < 2048; i++) halves[i] = i * 137 + 31;
  for (unsigned i = 0; i < 2048; i++) {
    if (halves[i] != (uint16_t)(i * 137 + 31)) halfPass = false;
  }
  result("PSRAM halfword writes across 4 KiB", halfPass);
#else
  SerialUSB.print("SKIP PSRAM: CH32V407 has none.\n\r");
#endif
}

void testSensor() {
  SensirionI2cStc42 sensor;
  sensor.begin(stemma, STC42A_I2C_ADDR_2B);
  int16_t error = sensor.stopMeasurement();
  uint32_t product = 0;
  int8_t serial[8] = {};
  error = sensor.readProductInfo(product, serial, sizeof(serial));
  result("STC42 identity and CRC", error == 0);
  if (error) { SerialUSB.print(error); SerialUSB.print("\n\r"); return; }
  SerialUSB.print("STC42 product=0x"); SerialUSB.print(product, HEX);
  SerialUSB.print(" serial=");
  for (uint8_t value : serial) { SerialUSB.print(value, HEX); SerialUSB.print(' '); }
  SerialUSB.print("\n\r");
  error = sensor.compensateHumidityWithSht41();
  if (!error) error = sensor.compensateTemperatureWithSht41();
  if (!error) error = sensor.startConcentrationMeasurement(1000);
  if (error) { result("STC42 start measurement", false); return; }
  for (unsigned i = 0; i < 3; i++) {
    delay(1100);
    float hydrogen, temperature, humidity;
    uint16_t pressure;
    STC42ReadErrorFlags flags = {};
    error = sensor.readMeasurements(hydrogen, temperature, humidity, pressure, flags);
    result("STC42 measurement and CRC", error == 0);
    if (!error) {
      SerialUSB.print("H2 vol%="); SerialUSB.print(hydrogen, 4);
      SerialUSB.print(" temperature C="); SerialUSB.print(temperature, 2);
      SerialUSB.print(" RH%="); SerialUSB.print(humidity, 2);
      SerialUSB.print(" pressure mbar="); SerialUSB.print(pressure);
      SerialUSB.print(" flags=0x"); SerialUSB.print(flags.value, HEX);
      SerialUSB.print("\n\r");
    }
  }
  sensor.stopMeasurement();
}

void testSDIO() {
  MetroSDIO card;
  bool pass = card.begin();
  result("SDIO four-bit initialization", pass);
  if (!pass) {
    SerialUSB.print("SDIO command="); SerialUSB.print(card.lastCommand());
    SerialUSB.print(" error=0x"); SerialUSB.print(card.error(), HEX);
    SerialUSB.print(" response=0x"); SerialUSB.print(card.response(), HEX);
    SerialUSB.print("\n\r"); return;
  }
  SerialUSB.print("SDIO sectors="); SerialUSB.print(card.sectorCount());
  SerialUSB.print(" clock Hz="); SerialUSB.print(card.clockHz());
  SerialUSB.print("\n\r");
  uint8_t block[512], again[512];
  pass = true;
  for (uint32_t i = 0; i < 16; i++) {
    if (!card.readSector(i, block) || !card.readSector(i, again) || memcmp(block, again, 512)) {
      pass = false; break;
    }
  }
  result("SDIO repeated sector reads and hardware CRC", pass);
  if (!pass) {
    SerialUSB.print("SDIO error=0x"); SerialUSB.print(card.error(), HEX);
    SerialUSB.print("\n\r"); card.end(); return;
  }
  FsVolume volume;
  pass = volume.begin(&card);
  result("SDIO filesystem mount", pass);
  if (pass) {
    SerialUSB.print("Filesystem type="); SerialUSB.print(volume.fatType());
    SerialUSB.print("\n\r");
    volume.end();
  }
  card.end();
}

void testSDFile() {
  MetroSDIO card;
  FsVolume volume;
  if (!card.begin() || !volume.begin(&card)) { result("SDIO file setup", false); return; }
  char name[13];
  FsFile file;
  bool opened = false;
  for (unsigned i = 0; i < 100; i++) {
    snprintf(name, sizeof(name), "METRO%02u.TST", i);
    if (!volume.exists(name)) {
      opened = file.open(name, O_RDWR | O_CREAT | O_EXCL);
      break;
    }
  }
  if (!opened) { result("SDIO new test file", false); volume.end(); card.end(); return; }
  uint8_t block[512];
  bool pass = true;
  for (unsigned sector = 0; sector < 64 && pass; sector++) {
    for (unsigned i = 0; i < 512; i++) block[i] = (i * 37 + sector * 13) % 256;
    pass = file.write(block, sizeof(block)) == sizeof(block);
  }
  pass &= file.sync();
  file.close();
  pass &= file.open(name, O_RDONLY);
  for (unsigned sector = 0; sector < 64 && pass; sector++) {
    pass = file.read(block, sizeof(block)) == sizeof(block);
    for (unsigned i = 0; i < 512; i++) {
      if (block[i] != (i * 37 + sector * 13) % 256) pass = false;
    }
  }
  pass &= file.fileSize() == 32768;
  file.close();
  result("SDIO new 32 KiB file write/reopen/read", pass);
  SerialUSB.print("Preserved test file: "); SerialUSB.print(name); SerialUSB.print("\n\r");
  volume.end(); card.end();
}

void testEthernet() {
  static bool started = false;
  if (!started) {
    // The PCB routes the internal PHY LEDs to PD14/PD15.
    ch32rv_clock_enable(AFIO);
    CH32RV_AFIO_PCFR1 |= 1u << 31;
    ch32rv_gpio_clock_enable(3);
    ch32rv_gpio_set_config(3, 14, CH32RV_GPIO_CFG_AF_PP_50M);
    ch32rv_gpio_set_config(3, 15, CH32RV_GPIO_CFG_AF_PP_50M);
    started = ETH.begin();
    result("Ethernet MAC/PHY initialization", started);
  }
  if (!started) return;
  uint32_t start = millis();
  while (!ETH.hasIP() && millis() - start < 15000) delay(10);
  result("Ethernet cable link", ETH.linkUp());
  if (!ETH.linkUp()) return;
  SerialUSB.print("Ethernet speed="); SerialUSB.print(ETH.linkSpeed());
  SerialUSB.print(ETH.fullDuplex() ? " Mbps full duplex" : " Mbps half duplex");
  SerialUSB.print(" MAC="); SerialUSB.print(ETH.macAddress());
  SerialUSB.print("\n\r");
  result("Ethernet DHCP", ETH.hasIP());
  if (!ETH.hasIP()) return;
  SerialUSB.print("Ethernet IP="); SerialUSB.print(ETH.localIP()); SerialUSB.print("\n\r");
  NetworkClient client;
  bool pass = client.connect("example.com", 80);
  if (pass) {
    client.print("GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n");
    start = millis();
    while (!client.available() && millis() - start < 3000) delay(10);
    String status = client.readStringUntil('\n');
    SerialUSB.print("HTTP: "); SerialUSB.print(status); SerialUSB.print("\n\r");
    pass = status.startsWith("HTTP/1.1 200") || status.startsWith("HTTP/1.0 200");
    client.stop();
  }
  result("Ethernet DNS/TCP/HTTP response", pass);
  SerialUSB.print("Ethernet RX="); SerialUSB.print(ETH.framesReceived());
  SerialUSB.print(" TX="); SerialUSB.print(ETH.framesSent());
  SerialUSB.print(" RX errors="); SerialUSB.print(ETH.receiveErrors());
  SerialUSB.print("\n\r");
}
