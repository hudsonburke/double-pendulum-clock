#include <Arduino.h>
#include <SimpleFOC.h>
#include <Preferences.h>
Preferences prefs;


#define M1_PWM1 D10
#define M1_PWM2 D9
#define M1_PWM3 D8

#define M1_EN D7
#define M1_PP 11
#define S1_SCL D1
#define S1_SDA D0

// #define M2_PWM1 11
// #define M2_PWM2 12
// #define M2_PWM3 13
// #define M2_EN 14


#define MAP_SIZE 1024
const int settle_cycles = 250;
const int sample_cycles = 80;
float calibration_map[MAP_SIZE];

constexpr float cogging_gain = 0.15f;
constexpr float cogging_deadband = 0.12f;
constexpr float cogging_angle_threshold = 0.005f;
constexpr unsigned long cogging_gate_window_ms = 50;
constexpr bool cogging_debug_enabled = true;
constexpr unsigned long cogging_debug_interval_ms = 100;
constexpr float voltage_limit_position    = 4.0f;
constexpr float voltage_limit_freeswing   = 2.0f;
constexpr float voltage_limit_sustain     = 2.0f;
constexpr float voltage_limit_calibration = 0.5f;
constexpr float sustain_torque = 0.3f;
constexpr float sustain_sign   = 1.0f;

BLDCMotor motor1 = BLDCMotor(M1_PP);
BLDCDriver3PWM driver1 = BLDCDriver3PWM(M1_PWM1, M1_PWM2, M1_PWM3, M1_EN);
MagneticSensorI2C sensor1 = MagneticSensorI2C(AS5600_I2C);

enum ControlMode { POSITION, FREE_SWING, FREE_SWING_SUSTAIN };
ControlMode current_mode  = FREE_SWING;
float       target_angle  = 0.0f;
bool        sustain_enabled = false;

void calibrateCogging();
bool loadCoggingMap();
bool saveCoggingMap();
float mapMean();


void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(S1_SDA, S1_SCL);

  Wire.setClock(400000);

  sensor1.init();

  driver1.voltage_power_supply = 12;
  // driver1.voltage_limit = 12;
  driver1.init();
  
  motor1.linkSensor(&sensor1);
  motor1.linkDriver(&driver1);
  motor1.controller = MotionControlType::torque;
  motor1.torque_controller = TorqueControlType::voltage;
  motor1.voltage_limit = 2.0f;
  motor1.init();
  motor1.initFOC();

  Serial.printf("[NVS] calibration_map size: %u bytes\n",
                (unsigned)sizeof(calibration_map));
  Serial.println("[NVS] Preferences diagnostics enabled");

  if (!loadCoggingMap()) {
    Serial.println("No valid cogging map found; running calibration");
    calibrateCogging();
    if (!saveCoggingMap()) {
      Serial.println("Failed to save cogging map");
    }
  } else {
    Serial.println("Loaded cogging map from NVS");
  }
}

void loop() {
  static unsigned long last_cogging_debug_ms = 0;
  static float gate_prev_angle = 0.0f;
  static unsigned long gate_prev_ms = 0;
  static bool gate_initialized = false;
  static bool gate_in_motion = false;

  sensor1.update();

  switch (current_mode) {
    case FREE_SWING: {
      float current_angle = fmod(sensor1.getAngle(), _2PI);
      if (current_angle < 0) current_angle += _2PI;

      float map_position = current_angle / _2PI * MAP_SIZE;
      int index0 = (int)map_position % MAP_SIZE;
      int index1 = (index0 + 1) % MAP_SIZE;
      float fraction = map_position - index0;

      float map_value = calibration_map[index0] * (1.0f - fraction) +
                        calibration_map[index1] * fraction;

      unsigned long now_ms = millis();
      bool in_motion = false;

      if (!gate_initialized) {
        gate_prev_angle = current_angle;
        gate_prev_ms = now_ms;
        gate_initialized = true;
      } else if (now_ms - gate_prev_ms >= cogging_gate_window_ms) {
        float delta = current_angle - gate_prev_angle;
        if (delta > _PI) delta -= _2PI;
        if (delta < -_PI) delta += _2PI;

        gate_in_motion = fabsf(delta) >= cogging_angle_threshold;

        gate_prev_angle = current_angle;
        gate_prev_ms = now_ms;
      }

      in_motion = gate_in_motion;

      float cogging_compensation = 0.0f;
      if (in_motion && fabsf(map_value) >= cogging_deadband) {
        cogging_compensation = -map_value * cogging_gain;
      }

      if (cogging_debug_enabled) {
        unsigned long now = millis();
        if (now - last_cogging_debug_ms >= cogging_debug_interval_ms) {
          last_cogging_debug_ms = now;
          Serial.printf("[COG] angle=%.4f map=%.4f comp=%.4f motion=%d\n",
                        current_angle,
                        map_value,
                        cogging_compensation,
                        (int)in_motion);
        }
      }

      motor1.move(cogging_compensation);
      motor1.loopFOC();
      break;
    }
    case POSITION:
      motor1.loopFOC();
      break;
    case FREE_SWING_SUSTAIN:
      motor1.loopFOC();
      break;
  }
}

void calibrateCogging() {
  motor1.controller = MotionControlType::angle;
  motor1.voltage_limit = 0.5f;

  for (int i = 0; i < MAP_SIZE; i++) {
    calibration_map[i] = 0.0f;
  }

  for (int i = 0; i < MAP_SIZE; i++) {
    float angle = (float)i / MAP_SIZE * _2PI;

    for (int s = 0; s < settle_cycles; s++) {
      motor1.move(angle);
      motor1.loopFOC();
      delayMicroseconds(300);
    }

    float accum = 0.0f;
    for (int s = 0; s < sample_cycles; s++) {
      motor1.move(angle);
      motor1.loopFOC();
      accum += motor1.voltage.q;
      delayMicroseconds(300);
    }

    // Use mean measurement
    calibration_map[i] = accum / sample_cycles;

    if ((i % 256) == 0) {
      Serial.printf("Calibrating: %d/%d\n", i, MAP_SIZE);
    }
  }

  // Subtract DC Bias
  float mean = mapMean();
  for (int i = 0; i < MAP_SIZE; i++) {
    calibration_map[i] -= mean;
  }

  motor1.controller = MotionControlType::torque;
  motor1.torque_controller = TorqueControlType::voltage;
  motor1.voltage_limit = 2.0f;
  Serial.println("Calibration complete");
}

float mapMean() {
  float sum = 0.0f;
  for (int i = 0; i < MAP_SIZE; i++) {
    sum += calibration_map[i];
  }
  return sum / MAP_SIZE;
}

bool loadCoggingMap() {
  if (!prefs.begin("cog_map", true)) {
    Serial.println("[NVS] FAIL: prefs.begin('cog_map', readonly) failed");
    return false;
  }

  size_t map_bytes = sizeof(calibration_map);
  if (!prefs.isKey("map")) {
    Serial.println("[NVS] LOAD: key 'map' does not exist");
    prefs.end();
    return false;
  }

  size_t stored = prefs.getBytesLength("map");
  if (stored != map_bytes) {
    Serial.printf("[NVS] LOAD: size mismatch - stored %u, expected %u\n",
                  (unsigned)stored, (unsigned)map_bytes);
    prefs.end();
    return false;
  }

  size_t got = prefs.getBytes("map", calibration_map, map_bytes);
  prefs.end();

  if (got != map_bytes) {
    Serial.printf("[NVS] LOAD: read %u of %u bytes\n",
                  (unsigned)got, (unsigned)map_bytes);
    return false;
  }

  Serial.printf("[NVS] LOAD: OK - %u bytes read\n", (unsigned)got);
  return true;
}

bool saveCoggingMap() {
  if (!prefs.begin("cog_map", false)) {
    Serial.println("[NVS] FAIL: prefs.begin('cog_map', readwrite) failed");
    return false;
  }

  size_t map_bytes = sizeof(calibration_map);
  size_t written = prefs.putBytes("map", calibration_map, map_bytes);
  prefs.end();

  if (written != map_bytes) {
    Serial.printf("[NVS] SAVE: wrote %u of %u bytes - FAILED\n",
                  (unsigned)written, (unsigned)map_bytes);
    return false;
  }

  Serial.printf("[NVS] SAVE: OK - %u bytes written\n", (unsigned)written);
  return true;
}
