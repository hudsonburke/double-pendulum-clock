#include <Arduino.h>
#include <SPI.h>
#include <SimpleFOC.h>
#include <SimpleFOCDrivers.h>
#include <encoders/mt6701/MagneticSensorMT6701SSI.h>

#include "pins.h"

#define DEBUG_ENABLED false

constexpr int M1_PP = 11;  // Pole pairs
constexpr int M2_PP = 11;  // Pole pairs
constexpr float power_supply_voltage = 12.0f;

enum ControlMode {
  DISABLED,
  COMPENSATION_ONLY,
  MINUTE_HAND_POSITION,
  HOUR_HAND_POSITION
};

// Motor 1 = Hour hand
BLDCMotor motor1 = BLDCMotor(M1_PP);
BLDCDriver3PWM driver1 = BLDCDriver3PWM(M1_PWM1, M1_PWM2, M1_PWM3, M1_EN);
MagneticSensorI2C sensor1 = MagneticSensorI2C(AS5600_I2C);

// Motor 2 = Minute hand
BLDCMotor motor2 = BLDCMotor(M2_PP);
BLDCDriver3PWM driver2 = BLDCDriver3PWM(M2_PWM1, M2_PWM2, M2_PWM3, M2_EN);
MagneticSensorMT6701SSI sensor2(SPI_CS);

ControlMode control_mode = DISABLED;

void setControlMode(ControlMode next_mode) {
  if (control_mode == next_mode) {
    return;
  }

  control_mode = next_mode;

  switch (control_mode) {
    case DISABLED:
      motor1.disable();
      motor2.disable();
      Serial.println("[MODE] DISABLED");
      break;
    case COMPENSATION_ONLY:
      motor1.controller = MotionControlType::torque;
      motor1.torque_controller = TorqueControlType::voltage;
      motor1.target = 0.0f;
      motor1.enable();

      motor2.controller = MotionControlType::torque;
      motor2.torque_controller = TorqueControlType::voltage;
      motor2.target = 0.0f;
      motor2.enable();
      Serial.println("[MODE] COMPENSATION ONLY");
      break;
    case MINUTE_HAND_POSITION:
      motor1.disable();
      motor2.controller = MotionControlType::angle;
      motor2.target = sensor2.getAngle();
      motor2.enable();
      Serial.println("[MODE] MINUTE HAND POSITION CONTROL");
      break;
    case HOUR_HAND_POSITION:
      motor2.disable();
      motor1.controller = MotionControlType::angle;
      motor1.target = sensor1.getAngle();
      motor1.enable();
      Serial.println("[MODE] HOUR HAND POSITION CONTROL");
      break;
  }
}

void applyControlMode() {
  switch (control_mode) {
    case DISABLED:
      break;
    case COMPENSATION_ONLY:
      motor1.move();
      motor2.move();
      break;
    case MINUTE_HAND_POSITION:
      motor2.move();
      break;
    case HOUR_HAND_POSITION:
      motor1.move();
      break;
  }
}

void setTargetAngle(float raw_target_degrees) {
  float normalized_target = fmod(raw_target_degrees, 360.0f);
  if (normalized_target < 0.0f) {
    normalized_target += 360.0f;
  }

  float target_radians = normalized_target * M_PI / 180.0f;
  if (control_mode == MINUTE_HAND_POSITION) {
    motor2.target = target_radians;
    Serial.printf("[TARGET] Minute hand target set to %.2f degrees\n",
                  normalized_target);
  } else if (control_mode == HOUR_HAND_POSITION) {
    motor1.target = target_radians;
    Serial.printf("[TARGET] Hour hand target set to %.2f degrees\n",
                  normalized_target);
  } else {
    Serial.println(
        "[ERROR] Target angle can only be set in position control modes");
  }
}

void controlModeParse() {
  static char buf[32];
  static int buf_len = 0;
  char c = Serial.read();

  if (c == '\n' || c == '\r') {
    if (buf_len == 0) {
      return;
    }

    buf[buf_len] = '\0';
    float raw_target = 0.0f;

    switch (buf[0]) {
      case 'd':
        setControlMode(DISABLED);
        break;
      case 'c':
        setControlMode(COMPENSATION_ONLY);
        break;
      case 'm':
        setControlMode(MINUTE_HAND_POSITION);
        break;
      case 'h':
        setControlMode(HOUR_HAND_POSITION);
        break;
      case 't':
        if (sscanf(buf + 1, "%f", &raw_target) == 1) {
          setTargetAngle(raw_target);
        } else {
          Serial.println("[ERROR] Invalid target angle format");
        }
        break;
      default:
        Serial.println("[ERROR] Unknown command");
        break;
    }

    buf_len = 0;
    return;
  }

  if (buf_len < static_cast<int>(sizeof(buf)) - 1) {
    buf[buf_len++] = c;
    return;
  }

  buf_len = 0;
  Serial.println("[ERROR] Command too long");
}

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for Serial to initialize

  Wire.begin();
  Wire.setClock(400000);  // 400kHz I2C clock speed to minimize blocking

  sensor1.init();

  driver1.voltage_power_supply = power_supply_voltage;
  driver1.init();

  motor1.linkSensor(&sensor1);
  motor1.linkDriver(&driver1);
  motor1.controller = MotionControlType::angle;
  motor1.init();
  motor1.initFOC();
  motor1.disable();

  sensor2.init();

  driver2.voltage_power_supply = power_supply_voltage;
  driver2.init();

  motor2.linkSensor(&sensor2);
  motor2.linkDriver(&driver2);
  motor2.controller = MotionControlType::angle;
  motor2.init();
  motor2.initFOC();
  motor2.disable();

  Serial.println("Initialization complete");
  Serial.println(
      "Enter 'd' to disable motors, 'c' for compensation only, 'm' for minute "
      "hand position control, 'h' for hour hand position control, and "
      "'t<angle>' to set target angle in position control modes (e.g. t90 to "
      "set target to 90 degrees)");
}

void loop() {
  motor1.loopFOC();
  motor2.loopFOC();

  while (Serial.available()) {
    controlModeParse();
  }

  applyControlMode();
}
