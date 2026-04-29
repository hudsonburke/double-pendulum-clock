/*
 * MT6701 SPI (SSI) Magnetic Encoder Diagnostic Sketch
 *
 * Purpose: Minimal initialization and continuous angle readout for manual
 * verification Protocol: SPI/SSI (Serial Synchronous Interface, NOT standard
 * SPI MODE) Resolution: 14-bit (0-16383 raw = 0-360°)
 *
 * IMPORTANT: MT6701 uses SPI_MODE3 (CPOL=1, CPHA=1)
 * Clock is idle HIGH, data captured on rising edge
 *
 * Teensy Wiring (standard SPI pins):
 *   MT6701 VDD   → Teensy 3.3V (or 5V for EEPROM programming)
 *   MT6701 GND   → Teensy GND
 *   MT6701 CLK   → Teensy SCK  (Teensy 3.x: pin 13)
 *   MT6701 DO    → Teensy MISO (Teensy 3.x: pin 12)
 *   MT6701 CSN   → Teensy CS   (Teensy 3.x: pin 10, or any GPIO)
 *   MT6701 MODE  → Leave floating (has internal 200kΩ pull-up for SSI mode)
 *
 * Magnet: 6mm x 2.5mm diametric magnet (radially magnetized)
 *
 * Data Format:
 *   3 bytes transferred per read:
 *   Byte 1: [D13:D6] of 14-bit angle
 *   Byte 2: [D5:D0] of 14-bit angle + status bits
 *   Byte 3: reserved / status info
 */

#include <SPI.h>

// MT6701 SPI Constants
#define MT6701_CS_PIN 10           // Chip select (adjust to your pin)
#define MT6701_SPI_SPEED 1000000   // 1 MHz (supports up to 15.625 MHz)
#define MT6701_SPI_MODE SPI_MODE2  // Clock idle HIGH, sample on rising edge

// Conversion constants
#define RAW_TO_DEGREES (360.0 / 16384.0)  // 14-bit resolution
#define RAW_TO_RADIANS ((2.0 * 3.14159265359) / 16384.0)

// State tracking for continuous rotation detection
uint16_t prev_raw = 0;
int32_t cumulative_angle = 0;  // Tracks multiple revolutions

// ===== MT6701 SPI Functions =====

/**
 * Read 14-bit angle from MT6701 via SPI/SSI
 *
 * Transfer 3 bytes:
 *   Byte 0: [D13:D6] of angle (8 bits)
 *   Byte 1: [D5:D0] of angle (6 bits) + status (2 bits)
 *   Byte 2: reserved
 *
 * 14-bit angle = (byte0 << 6) | (byte1 >> 2)
 *
 * Status bits in byte1[1:0]:
 *   00 = magnet strength OK
 *   01 = magnet too strong
 *   10 = magnet too weak
 *   11 = invalid
 */
uint16_t mt6701_readRawAngle() {
  uint8_t data[3] = {0, 0, 0};

  // Begin SPI transaction
  SPI.beginTransaction(
      SPISettings(MT6701_SPI_SPEED, MSBFIRST, MT6701_SPI_MODE));

  // Pull CS low to start transfer
  digitalWrite(MT6701_CS_PIN, LOW);
  delayMicroseconds(1);  // Setup time

  // Transfer 3 bytes (send zeros, receive angle data)
  data[0] = SPI.transfer(0);
  data[1] = SPI.transfer(0);
  data[2] = SPI.transfer(0);

  // Pull CS high to end transfer
  delayMicroseconds(1);
  digitalWrite(MT6701_CS_PIN, HIGH);

  SPI.endTransaction();

  // Extract 14-bit angle:
  // SSI frame starts with a leading '1' start bit, so data[0] = [1, A13:A7]
  // data[1] = [A6:A0, status bits]
  uint16_t raw = ((uint16_t)(data[0] & 0x7F) << 7) | (data[1] >> 1);

  return raw;
}

/**
 * Extract magnetic field status from last SPI read
 * Status bits in byte1[1:0]:
 *   00 = magnet strength OK
 *   01 = magnet too strong
 *   10 = magnet too weak
 *   11 = invalid
 */
uint8_t mt6701_getStatus(uint8_t byte1) { return byte1 & 0x03; }

/**
 * Verify MT6701 is responding by reading angle
 */
bool mt6701_testConnection() {
  // Read angle 5 times
  for (int i = 0; i < 5; i++) {
    uint16_t raw = mt6701_readRawAngle();
    // If we get any value in valid range and it changes, connection is good
    if (raw < 16384) {
      delay(20);
      uint16_t raw2 = mt6701_readRawAngle();
      // Different readings indicate sensor is responsive
      if (raw != raw2 || raw2 != 0) {
        return true;
      }
    }
  }
  return false;
}

void printHeader() {
  Serial.println("RAW\tDEGREES\t\tCUMULATIVE");
  Serial.println("-----\t-------\t\t----------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial monitor

  Serial.println("\n\n========================================");
  Serial.println("  MT6701 SPI Magnetic Encoder Diagnostic");
  Serial.println("========================================\n");

  // Initialize SPI
  SPI.begin();
  pinMode(MT6701_CS_PIN, OUTPUT);
  digitalWrite(MT6701_CS_PIN, HIGH);  // CS idle high
  delay(100);

  Serial.println("✓ SPI initialized");
  Serial.print("  Clock Speed: ");
  Serial.print(MT6701_SPI_SPEED / 1000000);
  Serial.println(" MHz (SPI_MODE2)");
  Serial.print("  CS Pin: ");
  Serial.println(MT6701_CS_PIN);

  // Test communication
  if (!mt6701_testConnection()) {
    Serial.println("\n⚠ No data received from MT6701");
    Serial.println("Check wiring: CSN, CLK, DO (MISO)");
    Serial.println("Ensure MODE pin is floating or pulled high");
    while (1) {
      delay(1000);
    }
  }

  Serial.println("✓ MT6701 communication verified\n");

  // Initialize state
  prev_raw = mt6701_readRawAngle();
  cumulative_angle = 0;

  Serial.println("✓ Initialization complete");
  Serial.println("\nRotate the magnet shaft and watch angle values change.");
  Serial.println(
      "Multiple columns: RAW (0-16383), DEGREES (0-360), CUMULATIVE (multiple "
      "turns)\n");

  printHeader();
}

void loop() {
  uint16_t raw_angle = mt6701_readRawAngle();

  // Detect wraparound for multi-turn tracking
  int16_t delta = (int16_t)(raw_angle - prev_raw);

  // If delta is very negative, we wrapped forward (crossed 16384)
  if (delta < -8192) {
    cumulative_angle++;  // Next revolution
  }
  // If delta is very positive, we wrapped backward
  else if (delta > 8192) {
    cumulative_angle--;  // Previous revolution
  }

  prev_raw = raw_angle;

  // Calculate angles
  float single_turn_degrees = raw_angle * RAW_TO_DEGREES;
  float cumulative_degrees = (cumulative_angle * 360.0) + single_turn_degrees;

  // Print in columns for easy monitoring
  Serial.print(raw_angle);
  Serial.print("\t");
  Serial.print(single_turn_degrees, 1);
  Serial.print("°\t");
  Serial.print(cumulative_degrees, 1);
  Serial.println("°");

  // delay(100);  // 10 Hz update rate
}
