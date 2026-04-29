// Motor Position Control Test
// Motor rotates continuously but:
// Stop at North (0°) → stay 3 sec
// Stop at East (90°) → stay 3 sec
// Stop at South (180°) → stay 3 sec
// Stop at West (270°) → stay 3 sec
// Other angles → pass through without stopping )
//  Arduino UNO R3 +  DRV8313 (Moto rdriver) +  AS5600 encoder (BLDC Motor)
//  Use the Simple FOC library
//  Connect DRV8313 to Arduino UNO: PWM pin 9 10 11, and enable 8
//  Connect AS5600 to Arduino UNO (I2C):SDA→A4, SCL→A5, VCC→5V, GND→GND

#include <SimpleFOC.h>
#include <Wire.h>

// Set pole pairs for the brushless DC motor
BLDCMotor motor = BLDCMotor(7);  // change pole pairs

// Driver (set pins for driver: DRV8313)
BLDCDriver3PWM driver = BLDCDriver3PWM(9, 10, 11, 8);

// AS5600 encoder
MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);

// Target angles
float targets[] = {0, _PI / 2, _PI, 3 * _PI / 2};

int targetIndex = 0;
unsigned long holdStart = 0;
bool holding = false;

void setup() {
  Wire.begin();

  sensor.init();
  motor.linkSensor(&sensor);

  driver.voltage_power_supply = 12;
  driver.init();
  motor.linkDriver(&driver);

  motor.controller = MotionControlType::angle;

  motor.init();
  motor.initFOC();
}

void loop() {
  motor.loopFOC();

  float target = targets[targetIndex];
  motor.move(target);

  float error = fabs(motor.shaft_angle - target);

  if (error < 0.05 && !holding) {
    holding = true;
    holdStart = millis();
  }

  if (holding) {
    if (millis() - holdStart > 3000) {
      holding = false;
      targetIndex++;
      if (targetIndex >= 4) targetIndex = 0;
    }
  }
}
