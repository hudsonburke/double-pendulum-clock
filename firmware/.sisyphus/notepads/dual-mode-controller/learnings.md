# Learnings - Dual Mode Controller

## [2026-04-08] Task: Task 1 complete
- Board: seeed_xiao_esp32s3, SimpleFOC v2.4.0, Arduino framework
- platformio.ini was wrong (teensy41 → seeed_xiao_esp32s3), now fixed
- constexpr bool cogging_debug_enabled MUST become plain bool for D command toggling (Task 3)
- All constexpr tuning constants live at lines 27-38 of main.cpp
- Enum ControlMode + globals at lines 44-47
- `_2PI` and `_PI` are SimpleFOC constants (no need to define)
- cogging_gate_window_ms, cogging_angle_threshold, cogging_deadband, cogging_gain all existing
- MAP_SIZE = 1024
- motor1 is torque/voltage mode at boot with voltage_limit=2.0f
- sensor1.getAngle() returns cumulative angle (not wrapped)
- NVS namespace is "cog_map", key is "map"

## Safe mode transitions
- Correct pattern: disable() → set controller → set voltage_limit → enable()
- NEVER direct motor1.controller assignment in loop()
- calibrateCogging() at line 153 shows old unsafe pattern (no disable/enable) - do not copy

## Mode-specific voltage limits
- POSITION: voltage_limit_position = 4.0f
- FREE_SWING: voltage_limit_freeswing = 2.0f
- SUSTAIN: voltage_limit_sustain = 2.0f
- CALIBRATION: voltage_limit_calibration = 0.5f
