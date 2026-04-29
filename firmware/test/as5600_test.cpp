/*
 * AS5600 I2C Magnetic Encoder Diagnostic Sketch
 *
 * Purpose: Minimal initialization and continuous angle readout for manual
 * verification Protocol: I2C (default address 0x36) Resolution: 12-bit (0-4095
 * raw = 0-360°)
 *
 * Teensy Wiring:
 *   AS5600 VCC  → Teensy 3.3V (or 5V depending on module)
 *   AS5600 GND  → Teensy GND
 *   AS5600 SDA  → Teensy I2C SDA (Teensy 3.x: pin 18, Teensy LC: pin 18)
 *   AS5600 SCL  → Teensy I2C SCL (Teensy 3.x: pin 19, Teensy LC: pin 19)
 *   AS5600 DIR  → GND (normal direction) or leave floating with internal
 * pull-up
 *
 * Magnet: 3-4mm diametric magnet, perpendicular to chip surface
 */

#include <Wire.h>

// AS5600 I2C Constants
#define AS5600_ADDRESS 0x36
#define AS5600_RAW_ANGLE_REG 0x0E  // 2-byte read, 0x0E (high), 0x0F (low)
#define AS5600_STATUS_REG 0x0B     // Magnetic field status
#define AS5600_MAGNET_LOW_BIT 0
#define AS5600_MAGNET_HIGH_BIT 3
#define AS5600_MAGNET_DETECT_BIT 4

// Conversion constants
#define RAW_TO_DEGREES (360.0 / 4096.0)
#define RAW_TO_RADIANS ((2.0 * 3.14159265359) / 4096.0)

// State tracking for continuous rotation detection
uint16_t prev_raw = 0;
int32_t cumulative_angle = 0;  // Tracks multiple revolutions
float total_degrees = 0.0;

// ===== Helper Functions =====

/**
 * Verify AS5600 is present on I2C bus
 */
bool as5600_isConnected() {
  Wire.beginTransmission(AS5600_ADDRESS);
  return Wire.endTransmission() == 0;
}

/**
 * Read 12-bit raw angle (0-4095)
 * Register 0x0E (high byte), 0x0F (low byte)
 * Only upper 12 bits are valid: (high << 8) | low, then >> 4 to get 12 bits
 */
uint16_t as5600_readRawAngle() {
  uint8_t data[2] = {0, 0};

  // Request 2 bytes starting at register 0x0E
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(AS5600_RAW_ANGLE_REG);
  if (Wire.endTransmission(false) != 0) {
    return 0;  // Error
  }

  // Read 2 bytes
  if (Wire.requestFrom(AS5600_ADDRESS, 2) != 2) {
    return 0;  // Error
  }

  data[0] = Wire.read();  // High byte
  data[1] = Wire.read();  // Low byte

  // Combine into 16-bit value, shift right 4 for 12-bit alignment
  uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
  raw = raw >> 4;  // Keep only 12 bits

  return raw;
}

/**
 * Read status register (0x0B)
 * Bits [4:3] = magnet strength (00=too weak, 01=ok, 10=too strong)
 * Bit [0] = magnet detected
 */
uint8_t as5600_readStatus() {
  Wire.beginTransmission(AS5600_ADDRESS);
  Wire.write(AS5600_STATUS_REG);
  if (Wire.endTransmission(false) != 0) {
    return 0xFF;  // Error
  }

  if (Wire.requestFrom(AS5600_ADDRESS, 1) != 1) {
    return 0xFF;  // Error
  }

  return Wire.read();
}

/**
 * Print human-readable magnet status
 */
void printMagnetStatus(uint8_t status) {
  if (status == 0xFF) {
    Serial.println("⚠ Could not read status register");
    return;
  }

  bool detected = (status & 0x20) != 0;     // Bit 5
  uint8_t strength = (status >> 3) & 0x03;  // Bits [4:3]

  Serial.print("Magnet Status: ");
  if (!detected) {
    Serial.println("⚠ NOT DETECTED - check magnet placement");
  } else {
    Serial.print("✓ Detected | Strength: ");
    switch (strength) {
      case 0:
        Serial.println("TOO WEAK");
        break;
      case 1:
        Serial.println("OK");
        break;
      case 2:
        Serial.println("TOO STRONG");
        break;
      default:
        Serial.println("INVALID");
    }
  }
}

void printHeader() {
  Serial.println("RAW\tDEGREES\t\tCUMULATIVE");
  Serial.println("----\t-------\t\t----------");
}

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for serial monitor

  Serial.println("\n\n========================================");
  Serial.println("  AS5600 I2C Magnetic Encoder Diagnostic");
  Serial.println("========================================\n");

  // Initialize I2C
  Wire.begin();
  delay(100);

  // Verify connection
  if (!as5600_isConnected()) {
    Serial.println("ERROR: AS5600 not found at address 0x36");
    Serial.println("Check wiring: SDA, SCL, VCC, GND");
    while (1) {
      Serial.println(".");
      delay(1000);
    }
  }

  Serial.println("✓ AS5600 detected at I2C address 0x36");

  // Check magnet status
  uint8_t status = as5600_readStatus();
  printMagnetStatus(status);

  // Initialize state
  prev_raw = as5600_readRawAngle();
  cumulative_angle = 0;

  Serial.println("\n✓ Initialization complete");
  Serial.println("\nRotate the magnet shaft and watch angle values change.");
  Serial.println(
      "Multiple columns: RAW (0-4095), DEGREES (0-360), CUMULATIVE (multiple "
      "turns)\n");

  printHeader();
}

void loop() {
  uint16_t raw_angle = as5600_readRawAngle();

  // Detect wraparound for multi-turn tracking
  int16_t delta = (int16_t)(raw_angle - prev_raw);

  // If delta is very negative, we wrapped forward (crossed 4096)
  if (delta < -2048) {
    cumulative_angle++;  // Next revolution
  }
  // If delta is very positive, we wrapped backward
  else if (delta > 2048) {
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

  delay(100);  // 10 Hz update rate
}
