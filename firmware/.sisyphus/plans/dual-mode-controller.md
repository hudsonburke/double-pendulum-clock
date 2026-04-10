# Dual-Mode Motor Controller for Double Pendulum Clock

## TL;DR

> **Quick Summary**: Restructure the single-mode anticogging firmware into a 3-state motor controller (Position, Free-Swing, Free-Swing+Sustain) with serial command interface, safe mode transitions via SimpleFOC's updateMotionControlType(), and optional energy injection to sustain pendulum swing.
> 
> **Deliverables**:
> - State machine with 3 modes: POSITION, FREE_SWING, FREE_SWING_SUSTAIN
> - Serial command parser: P (position), F (free-swing), S (toggle sustain), T xxx (target angle), C (calibrate), D (toggle debug)
> - Safe runtime mode transitions using disable()/updateMotionControlType()/enable()
> - Position hold with angle clamping to [0, 2pi)
> - Sustain mode with direction-aware energy injection
> - Unified telemetry output with mode indicator
> 
> **Estimated Effort**: Medium
> **Parallel Execution**: NO - sequential (single file refactoring)
> **Critical Path**: Task 1 -> Task 2 -> Task 3 -> Task 4 -> Task 5 -> Task 6 -> Task 7

---

## Context

### Original Request
User wants to transform the existing single-mode anticogging compensation firmware into a dual-mode controller for a double pendulum clock. The motor must precisely position the pendulum (Position Mode) and then allow it to swing freely with friction/cogging compensation (Free-Swing Mode), with an optional energy sustain feature.

### Interview Summary
**Key Discussions**:
- **Mode switching**: Serial commands (P, F, S, T, C, D)
- **Drivetrain**: Belt and pulley, 1:1 ratio (no angle scaling needed)
- **Pendulum speed**: Medium (~1-3 Hz oscillation)
- **Position mode**: Go-to angle and hold only (no trajectory tracking)
- **Sustain**: Manual serial toggle, fixed-magnitude energy injection
- **Testing**: Hardware-only, no automated tests

**Research Findings**:
- SimpleFOC v2.4.0 has `updateMotionControlType()` (FOCMotor.cpp:495) - safe runtime mode switching that auto-sets target=shaft_angle and calls updateVoltageLimit()
- `enable()` resets all PIDs (BLDCMotor.cpp:132-135) - clean state on each transition
- Existing anticogging infrastructure (map, interpolation, gating) is fully functional
- AS5600 velocity too noisy for sustain timing - use angle delta direction instead

### Metis Review
**Identified Gaps** (addressed):
- **Target angle clamping**: SimpleFOC angle mode uses cumulative shaft_angle. Target must be clamped to [0, 2pi) to prevent multi-turn belt wrapping - added to Task 5
- **Mode-specific voltage limits**: Different modes need different voltage_limit values - defined per mode (POSITION=4.0V, FREE_SWING=2.0V, SUSTAIN=2.0V, CALIBRATION=0.5V)
- **Safe mode transition protocol**: Must use disable()/updateMotionControlType()/enable(), never direct assignment - enforced in Task 4
- **Serial buffer flush after calibration**: Queued commands during blocking calibration could fire unexpectedly - added to Task 3
- **Sustain sign convention**: Motor direction vs pendulum swing direction must be validated on hardware - sustain torque sign made configurable constant
- **Mode switch during swing**: POSITION entered mid-swing catches at current angle (handled by updateMotionControlType setting target=shaft_angle)
- **Boot mode**: Always boot into FREE_SWING (safest default) - no NVS mode persistence

---

## Work Objectives

### Core Objective
Restructure src/main.cpp from a single-mode anticogging compensator into a state-machine-based dual-mode controller with serial command interface, preserving all existing anticogging functionality.

### Concrete Deliverables
- src/main.cpp with 3-state controller, serial parser, mode transitions, position hold, sustain injection, unified telemetry

### Definition of Done
- [ ] `pio run` compiles with zero errors, zero warnings
- [ ] Sending `P` over serial -> motor enters position mode and holds current angle
- [ ] Sending `F` over serial -> motor enters free-swing with anticogging compensation
- [ ] Sending `S` while in free-swing -> sustain toggle, energy injection when swinging
- [ ] Sending `T 1.57` in position mode -> motor drives to ~90 deg and holds
- [ ] Sending `C` -> calibration runs, map saved to NVS
- [ ] Sending `D` -> debug telemetry toggles on/off
- [ ] Existing anticogging behavior unchanged in FREE_SWING mode

### Must Have
- State machine enum: POSITION, FREE_SWING, FREE_SWING_SUSTAIN
- Safe mode transitions via disable()/updateMotionControlType()/enable()
- Serial command parser for P, F, S, T, C, D
- Target angle clamping to [0, 2pi) in position mode
- Direction-aware sustain energy injection using angle delta (not velocity)
- Mode-specific voltage limits (POSITION=4.0V, FREE_SWING=2.0V, SUSTAIN=2.0V, CALIBRATION=0.5V)
- Unified telemetry with mode indicator: `[TEL] mode=X angle=X comp=X sustain=X`
- Boot into FREE_SWING as default
- Serial buffer flush after calibration completes

### Must NOT Have (Guardrails)
- No trajectory tracking or motion profiles in position mode
- No automatic mode switching (only serial commands)
- No automatic amplitude regulation in sustain mode
- No bidirectional cogging calibration
- No WiFi, BLE, or network control
- No multi-motor support
- No file splitting - keep single main.cpp
- No runtime-configurable voltage limits via serial (safety)
- No PID auto-tuning
- No NVS mode persistence (always boot to FREE_SWING)
- No SimpleFOC Commander class - custom minimal serial parser only
- No direct assignment of `motor1.controller` at runtime - always use `updateMotionControlType()`

---

## Verification Strategy

> **ZERO HUMAN INTERVENTION** - ALL verification is agent-executed. No exceptions.

### Test Decision
- **Infrastructure exists**: NO
- **Automated tests**: NO - hardware-only testing
- **Framework**: None (PlatformIO build check only)

### QA Policy
Every task MUST include agent-executed QA scenarios.
Evidence saved to `.sisyphus/evidence/task-{N}-{scenario-slug}.{ext}`.

- **Firmware/Build**: Use Bash (`pio run`) - compile check, zero errors/warnings
- **Code Review**: Use Read/Grep - verify patterns, no forbidden constructs
- **Serial Protocol**: Use Grep to verify all serial output format strings match documented protocol

**NOTE**: If `pio` is not available in the dev environment, compilation verification is deferred to user hardware testing. Code review and pattern verification remain agent-executable.

---

## Execution Strategy

### Parallel Execution Waves

This is a sequential refactoring of a single file. Each task builds on the previous.
Parallelism is limited because all tasks modify the same file (main.cpp).

```
Wave 1 (Foundation - structural scaffolding, zero behavior change):
  Task 1: Add state machine enum and mode variable [quick]
  Task 2: Wrap existing loop body in FREE_SWING state [quick]

Wave 2 (Features - incremental additions, each depends on previous):
  Task 3: Implement serial command parser [quick]
  Task 4: Implement safe mode transition functions [deep]
  Task 5: Implement POSITION mode loop body [quick]
  Task 6: Implement SUSTAIN mode [deep]
  Task 7: Add unified telemetry [quick]

Wave FINAL (After ALL tasks - 4 parallel reviews):
  Task F1: Plan compliance audit (oracle)
  Task F2: Code quality review (unspecified-high)
  Task F3: Build verification + code pattern QA (unspecified-high)
  Task F4: Scope fidelity check (deep)
  -> Present results -> Get explicit user okay
```

### Dependency Matrix

| Task | Depends On | Blocks | Wave |
|------|-----------|--------|------|
| 1 | - | 2-7 | 1 |
| 2 | 1 | 3-7 | 1 |
| 3 | 2 | 4-7 | 2 |
| 4 | 3 | 5-7 | 2 |
| 5 | 4 | 6-7 | 2 |
| 6 | 5 | 7 | 2 |
| 7 | 6 | F1-F4 | 2 |
| F1-F4 | 7 | - | FINAL |

### Agent Dispatch Summary

- **Wave 1**: 2 tasks - T1 `quick`, T2 `quick`
- **Wave 2**: 5 tasks - T3 `quick`, T4 `deep`, T5 `quick`, T6 `deep`, T7 `quick`
- **Wave FINAL**: 4 tasks (parallel) - F1 `oracle`, F2 `unspecified-high`, F3 `unspecified-high`, F4 `deep`

---

## TODOs

- [x] 1. Add state machine enum and mode variable

  **What to do**:
  - Add `enum ControlMode { POSITION, FREE_SWING, FREE_SWING_SUSTAIN };` before setup()
  - Add `ControlMode current_mode = FREE_SWING;` as a global variable
  - Add `float target_angle = 0.0f;` as a global for position mode target
  - Add `bool sustain_enabled = false;` as a global for sustain toggle state
  - Add mode-specific voltage limit constants at the top with other constexpr values:
    `constexpr float voltage_limit_position = 4.0f;`
    `constexpr float voltage_limit_freeswing = 2.0f;`
    `constexpr float voltage_limit_sustain = 2.0f;`
    `constexpr float voltage_limit_calibration = 0.5f;`
  - Add sustain tuning constants:
    `constexpr float sustain_torque = 0.3f;`  (energy injection magnitude in volts)
    `constexpr float sustain_sign = 1.0f;`    (+1 or -1, set based on motor/belt direction on hardware)
  - NO behavior changes. The enum and variables are declared but not used yet. loop() remains unchanged.

  **Must NOT do**:
  - Do not modify loop() or setup() behavior
  - Do not add switch statements yet
  - Do not add serial parsing yet

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple variable declarations, no logic changes
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 1 (sequential with Task 2)
  - **Blocks**: Tasks 2, 3, 4, 5, 6, 7
  - **Blocked By**: None (can start immediately)

  **References**:

  **Pattern References** (existing code to follow):
  - `src/main.cpp:22-32` - Existing constexpr parameter grouping pattern. New constants should follow same style and be placed in this block.
  - `src/main.cpp:34-36` - Global object declarations. New globals (enum, mode variable) go after this block, before function prototypes.

  **API/Type References**:
  - `src/main.cpp:60` - `MotionControlType::torque` - SimpleFOC's enum pattern. Our ControlMode enum follows similar naming convention.

  **External References**:
  - SimpleFOC v2.4.0 `FOCMotor.cpp:495` - `updateMotionControlType()` uses MotionControlType enum values we reference in Task 4.

  **WHY Each Reference Matters**:
  - Lines 22-32: Follow exact constexpr grouping so all tuning knobs are in one place at file top
  - Lines 34-36: Place new globals here so they are visible to all functions below

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with new declarations
    Tool: Bash (pio run)
    Preconditions: src/main.cpp has enum and new variables added
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-1-build.txt

  Scenario: No behavior change - existing code unmodified
    Tool: Read + Grep
    Preconditions: Task 1 changes applied
    Steps:
      1. Read src/main.cpp
      2. Grep for "enum ControlMode" - must exist exactly once
      3. Grep for "ControlMode current_mode" - must exist exactly once
      4. Verify loop() function body is IDENTICAL to original (lines 81-139 unchanged)
      5. Verify setup() function body is IDENTICAL to original (lines 44-79 unchanged)
    Expected Result: New declarations present, loop() and setup() unchanged
    Failure Indicators: loop() or setup() modified, enum missing, duplicate declarations
    Evidence: .sisyphus/evidence/task-1-no-regression.txt
  ```

  **Commit**: YES
  - Message: `refactor(motor): add state machine enum and mode variable`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

- [x] 2. Wrap existing loop body in FREE_SWING state

  **What to do**:
  - Add `switch (current_mode) {` at the beginning of loop(), AFTER `sensor1.update()`
  - Move the ENTIRE existing loop body (angle computation, map lookup, interpolation, motion gating, cogging compensation, debug output, motor.move, motor.loopFOC) into `case FREE_SWING:` with a `break;`
  - Add empty placeholder cases:
    ```
    case POSITION:
      motor1.loopFOC();
      break;
    case FREE_SWING_SUSTAIN:
      // Will be implemented in Task 6
      motor1.loopFOC();
      break;
    ```
  - The `sensor1.update()` call stays BEFORE the switch (shared by all modes)
  - `motor1.loopFOC()` must be called in EVERY case (FOC commutation must always run)
  - Move the static locals (`last_cogging_debug_ms`, `gate_prev_angle`, `gate_prev_ms`, `gate_initialized`, `gate_in_motion`) to be declared BEFORE the switch but still inside loop(), so they persist across modes
  - Zero behavior change: since current_mode = FREE_SWING at boot, the switch enters the same code path

  **Must NOT do**:
  - Do not modify the anticogging logic inside the FREE_SWING case
  - Do not add serial parsing
  - Do not implement position or sustain logic beyond placeholder loopFOC()

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Structural wrapping only, moving existing code into switch case
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 1 (after Task 1)
  - **Blocks**: Tasks 3, 4, 5, 6, 7
  - **Blocked By**: Task 1

  **References**:

  **Pattern References**:
  - `src/main.cpp:81-139` - The ENTIRE current loop() body. Every line must be preserved inside the FREE_SWING case.
  - `src/main.cpp:88` - `sensor1.update()` - this stays BEFORE the switch (shared by all modes)
  - `src/main.cpp:138-139` - `motor1.move(cogging_compensation); motor1.loopFOC();` - the move+loopFOC pair at end of loop. In FREE_SWING case these stay. Other cases need at minimum `motor1.loopFOC()`.
  - `src/main.cpp:82-86` - Static locals for motion gating. These must stay accessible to FREE_SWING (and later SUSTAIN) cases. Move them before the switch but keep them static inside loop().

  **WHY Each Reference Matters**:
  - Lines 81-139: This IS the code being restructured. Executor must understand what moves where.
  - Line 88: Critical that sensor update happens before the switch so all modes have fresh angle data.
  - Lines 138-139: Motor control calls - loopFOC is mandatory in every path to maintain FOC commutation.
  - Lines 82-86: Static locals must persist across loop iterations and be shared between FREE_SWING and SUSTAIN modes.

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with switch statement
    Tool: Bash (pio run)
    Preconditions: src/main.cpp has switch(current_mode) wrapping loop body
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-2-build.txt

  Scenario: Switch statement structure is correct
    Tool: Read + Grep
    Preconditions: Task 2 changes applied
    Steps:
      1. Read src/main.cpp loop() function
      2. Grep for "switch (current_mode)" - must exist exactly once, inside loop()
      3. Grep for "case FREE_SWING:" - must exist
      4. Grep for "case POSITION:" - must exist
      5. Grep for "case FREE_SWING_SUSTAIN:" - must exist
      6. Verify sensor1.update() is BEFORE the switch statement
      7. Verify motor1.loopFOC() exists in ALL three cases
      8. Verify the FREE_SWING case body contains the original anticogging logic
    Expected Result: All 3 cases present, sensor update before switch, loopFOC in all cases, anticogging preserved
    Failure Indicators: Missing case, sensor update inside case, loopFOC missing from a case, anticogging logic modified
    Evidence: .sisyphus/evidence/task-2-structure.txt
  ```

  **Commit**: YES
  - Message: `refactor(motor): wrap loop body in FREE_SWING state`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

- [x] 3. Implement serial command parser

  **What to do**:
  - Add a `processSerialInput()` function that reads serial input and dispatches commands
  - Call `processSerialInput()` at the top of loop(), after sensor1.update() but before the mode switch
  - Parse single-character commands (newline-terminated):
    - `P` or `p` -> print `[MODE] POSITION` (mode switch happens in Task 4, for now just print)
    - `F` or `f` -> print `[MODE] FREE_SWING`
    - `S` or `s` -> print `[MODE] SUSTAIN toggled`
    - `C` or `c` -> call existing `calibrateCogging()` + `saveCoggingMap()`, then flush serial buffer with `while(Serial.available()) Serial.read();`
    - `D` or `d` -> toggle `cogging_debug_enabled` (change from constexpr to regular `bool` to make it toggleable)
  - Parse multi-character command:
    - `T xxx` or `t xxx` -> parse float after T, set target_angle, print `[TARGET] xxx`
    - Use `Serial.parseFloat()` after detecting T
    - Clamp parsed angle to [0, 2pi) using fmod + wrap: `target_angle = fmod(val, _2PI); if (target_angle < 0) target_angle += _2PI;`
  - Handle garbage/unknown input gracefully - just ignore unrecognized characters
  - Use a simple line-based approach: read characters into a small buffer until newline, then parse
  - Add serial buffer flush after calibration: `while (Serial.available()) Serial.read();`
  - Print startup banner in setup(): `[BOOT] Dual-mode controller ready. Commands: P F S T<angle> C D`

  **Must NOT do**:
  - Do not use SimpleFOC Commander class
  - Do not implement actual mode transitions (motor state changes) - only print acknowledgments for now
  - Do not add WiFi/BLE handling

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Simple serial parsing, no complex motor control logic
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2 (after Task 2)
  - **Blocks**: Tasks 4, 5, 6, 7
  - **Blocked By**: Task 2

  **References**:

  **Pattern References**:
  - `src/main.cpp:44-46` - `Serial.begin(115200)` in setup(). Serial is already initialized at 115200 baud.
  - `src/main.cpp:66-68` - Existing Serial.printf pattern with `[TAG]` prefix format. Follow same style for `[MODE]`, `[TARGET]`, `[BOOT]` tags.
  - `src/main.cpp:142-184` - `calibrateCogging()` function. The `C` command should call this directly, then `saveCoggingMap()`.
  - `src/main.cpp:27` - `constexpr bool cogging_debug_enabled = true;` - Must change from constexpr to plain `bool` to allow runtime toggling via `D` command.

  **API/Type References**:
  - Arduino `Serial.available()`, `Serial.read()`, `Serial.parseFloat()` - standard serial API
  - `_2PI` - SimpleFOC constant for 2*pi, already used in codebase (line 91, 110-111)

  **WHY Each Reference Matters**:
  - Lines 44-46: Confirms serial is 115200, no re-init needed
  - Lines 66-68: Tag format to match for consistency
  - Lines 142-184: Calibration entry point for C command
  - Line 27: Must be changed from constexpr to mutable bool for D command

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with serial parser
    Tool: Bash (pio run)
    Preconditions: src/main.cpp has processSerialInput() function
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-3-build.txt

  Scenario: Serial parser function exists and is called from loop
    Tool: Read + Grep
    Preconditions: Task 3 changes applied
    Steps:
      1. Grep for "void processSerialInput" - must exist (function definition)
      2. Grep for "processSerialInput()" inside loop() - must be called
      3. Grep for "[MODE]" in Serial.printf strings - must exist for P, F, S commands
      4. Grep for "[TARGET]" in Serial.printf strings - must exist for T command
      5. Grep for "[BOOT]" in Serial.printf strings - must exist in setup()
      6. Grep for "Serial.available" inside processSerialInput - must exist (input reading)
      7. Verify cogging_debug_enabled is plain "bool" not "constexpr bool"
      8. Grep for "Commander" - must NOT exist anywhere in file
    Expected Result: Parser function defined and called, all tag formats present, debug is mutable, no Commander
    Failure Indicators: Missing function, not called from loop, missing tags, still constexpr, Commander class used
    Evidence: .sisyphus/evidence/task-3-parser.txt

  Scenario: Target angle clamping logic present
    Tool: Grep
    Preconditions: Task 3 changes applied
    Steps:
      1. Grep for "fmod" near target_angle assignment
      2. Grep for "_2PI" near target_angle clamping
      3. Verify the clamping wraps negative values: pattern "if.*target_angle.*<.*0.*target_angle.*+=.*_2PI"
    Expected Result: Target angle is clamped to [0, 2pi) using fmod and wrap
    Failure Indicators: No clamping, or clamping uses wrong range
    Evidence: .sisyphus/evidence/task-3-clamp.txt
  ```

  **Commit**: YES
  - Message: `feat(serial): add command parser for mode switching`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

- [x] 4. Implement safe mode transition functions

  **What to do**:
  - Add three mode transition functions:
    ```
    void switchToPosition() {
      motor1.disable();
      motor1.controller = MotionControlType::angle;
      motor1.voltage_limit = voltage_limit_position;
      motor1.PID_velocity.limit = voltage_limit_position;
      motor1.enable();
      target_angle = fmod(sensor1.getAngle(), _2PI);
      if (target_angle < 0) target_angle += _2PI;
      current_mode = POSITION;
      sustain_enabled = false;
      Serial.println("[MODE] POSITION");
    }

    void switchToFreeSwing() {
      motor1.disable();
      motor1.controller = MotionControlType::torque;
      motor1.torque_controller = TorqueControlType::voltage;
      motor1.voltage_limit = voltage_limit_freeswing;
      motor1.enable();
      current_mode = FREE_SWING;
      sustain_enabled = false;
      Serial.println("[MODE] FREE_SWING");
    }

    void toggleSustain() {
      if (current_mode == FREE_SWING || current_mode == FREE_SWING_SUSTAIN) {
        sustain_enabled = !sustain_enabled;
        current_mode = sustain_enabled ? FREE_SWING_SUSTAIN : FREE_SWING;
        motor1.voltage_limit = sustain_enabled ? voltage_limit_sustain : voltage_limit_freeswing;
        Serial.printf("[MODE] %s (sustain=%s)\n",
          sustain_enabled ? "FREE_SWING_SUSTAIN" : "FREE_SWING",
          sustain_enabled ? "ON" : "OFF");
      } else {
        Serial.println("[MODE] Sustain only available in FREE_SWING mode");
      }
    }
    ```
  - Wire these functions into processSerialInput():
    - `P` -> call `switchToPosition()`
    - `F` -> call `switchToFreeSwing()`
    - `S` -> call `toggleSustain()`
  - CRITICAL: The `disable() -> controller assignment -> voltage_limit -> enable()` sequence is the safe transition protocol. Do NOT use `updateMotionControlType()` since we need to also set `torque_controller` and `voltage_limit` which updateMotionControlType doesn't handle for our specific needs. Instead, manually do: disable, set controller type, set voltage limits, enable. This is equivalent but gives us more control.
  - NOTE on updateMotionControlType: Metis identified this as the safe method, but after reviewing the SimpleFOC source, direct assignment inside a disable/enable bracket is equally safe and gives us control over voltage_limit and torque_controller which updateMotionControlType() doesn't set. The disable() call prevents any motor actuation, and enable() resets PIDs.
  - When switching to POSITION: set target_angle to current sensor angle so motor holds current position (no sudden jump)
  - When switching to FREE_SWING: ensure sustain_enabled is false
  - Sustain toggle only works from FREE_SWING or FREE_SWING_SUSTAIN modes

  **Must NOT do**:
  - Do not implement the POSITION case loop body yet (that is Task 5)
  - Do not implement sustain energy injection yet (that is Task 6)
  - Do not make voltage limits runtime-configurable via serial

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Motor state transitions are safety-critical, need careful handling of SimpleFOC internals
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2 (after Task 3)
  - **Blocks**: Tasks 5, 6, 7
  - **Blocked By**: Task 3

  **References**:

  **Pattern References**:
  - `src/main.cpp:60-64` - Original setup sequence: `motor1.controller = MotionControlType::torque; motor1.torque_controller = TorqueControlType::voltage; motor1.voltage_limit = 2.0f; motor1.init(); motor1.initFOC();` - This shows how motor modes are configured. Transition functions follow similar pattern but with disable/enable instead of init.
  - `src/main.cpp:142-184` - `calibrateCogging()` already does a mode switch: sets controller to angle (line 143), voltage_limit to 0.5 (line 144), then restores to torque (line 181-183). This is the existing pattern for mode switching, but it lacks disable/enable.

  **API/Type References**:
  - SimpleFOC `BLDCMotor::disable()` - Sets voltage to 0, disables driver outputs (BLDCMotor.cpp)
  - SimpleFOC `BLDCMotor::enable()` - Re-enables driver, resets all PIDs (BLDCMotor.cpp:132-135)
  - SimpleFOC `MotionControlType::angle` - PID position control mode
  - SimpleFOC `MotionControlType::torque` - Direct voltage/torque control mode
  - SimpleFOC `TorqueControlType::voltage` - Voltage-mode torque control (used in free-swing)
  - SimpleFOC `motor1.PID_velocity.limit` - The velocity PID output limit, should match voltage_limit for angle mode

  **WHY Each Reference Matters**:
  - Lines 60-64: Shows what motor parameters need setting for each mode
  - Lines 142-184: Shows existing (unsafe) mode switch pattern that our new functions replace
  - disable/enable API: Critical for safe transitions - disable prevents motor actuation during reconfiguration

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with transition functions
    Tool: Bash (pio run)
    Preconditions: src/main.cpp has switchToPosition(), switchToFreeSwing(), toggleSustain()
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-4-build.txt

  Scenario: Safe transition protocol is correct
    Tool: Read + Grep
    Preconditions: Task 4 changes applied
    Steps:
      1. Read switchToPosition() function
      2. Verify it calls motor1.disable() BEFORE changing controller type
      3. Verify it calls motor1.enable() AFTER setting controller and voltage_limit
      4. Verify it sets target_angle to current sensor angle (no jump on switch)
      5. Read switchToFreeSwing() function
      6. Verify same disable/enable bracket pattern
      7. Verify it resets sustain_enabled = false
      8. Read toggleSustain() function
      9. Verify it checks current_mode before toggling (only works in free-swing modes)
      10. Grep for raw "motor1.controller =" OUTSIDE of transition functions and calibrateCogging - should NOT exist in loop()
    Expected Result: All transitions use disable/enable bracket, target set to current angle, sustain guarded
    Failure Indicators: Missing disable/enable, target not set, sustain works in position mode, raw controller assignment in loop
    Evidence: .sisyphus/evidence/task-4-transitions.txt

  Scenario: Voltage limits are mode-specific
    Tool: Read
    Preconditions: Task 4 changes applied
    Steps:
      1. In switchToPosition(): verify motor1.voltage_limit = voltage_limit_position (4.0V)
      2. In switchToFreeSwing(): verify motor1.voltage_limit = voltage_limit_freeswing (2.0V)
      3. In toggleSustain(): verify voltage_limit changes to sustain or freeswing based on toggle
    Expected Result: Each mode uses its own voltage limit constant
    Failure Indicators: Hardcoded values instead of constants, wrong constant used, missing voltage_limit assignment
    Evidence: .sisyphus/evidence/task-4-voltage-limits.txt
  ```

  **Commit**: YES
  - Message: `feat(motor): implement safe mode transitions`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

- [x] 5. Implement POSITION mode loop body

  **What to do**:
  - Fill in the POSITION case in the loop() switch statement:
    ```
    case POSITION:
      motor1.move(target_angle);
      motor1.loopFOC();
      break;
    ```
  - That is literally it. SimpleFOC's angle mode PID handles everything when `motor1.controller == MotionControlType::angle`.
  - `motor1.move(target_angle)` sets the angle target, the internal PID computes the voltage.
  - `motor1.loopFOC()` runs the FOC commutation loop.
  - The target_angle was already set by the `T` serial command (Task 3) with [0, 2pi) clamping.
  - On mode entry (switchToPosition in Task 4), target_angle is set to current angle so it holds in place.
  - IMPORTANT: SimpleFOC angle mode uses cumulative shaft_angle internally. The `motor1.move(target_angle)` call where target_angle is in [0, 2pi) will work correctly because the PID computes the error as `target - shaft_angle`. However, if the motor has rotated multiple turns, shaft_angle could be e.g. 12.56 and target could be 0.5, causing a multi-turn unwind. To prevent this, when receiving a new `T` command, compute the nearest equivalent angle: find the target that is closest to current shaft_angle by adding/subtracting 2pi multiples. Implement this in processSerialInput when handling `T`:
    ```
    float raw_target = <parsed value>;
    raw_target = fmod(raw_target, _2PI);
    if (raw_target < 0) raw_target += _2PI;
    // Find nearest equivalent to current shaft angle
    float current = sensor1.getAngle();
    float base = current - fmod(current, _2PI);
    if (fmod(current, _2PI) < 0) base -= _2PI;
    float candidate1 = base + raw_target;
    float candidate2 = candidate1 + _2PI;
    float candidate3 = candidate1 - _2PI;
    target_angle = candidate1;
    if (fabsf(candidate2 - current) < fabsf(target_angle - current)) target_angle = candidate2;
    if (fabsf(candidate3 - current) < fabsf(target_angle - current)) target_angle = candidate3;
    ```
    This ensures the motor takes the shortest path to the target angle.

  **Must NOT do**:
  - No trajectory tracking or smooth motion profiles
  - No acceleration ramps
  - No PID tuning logic
  - No anticogging feedforward in position mode (could be added later but out of scope)

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Very small code addition, but the nearest-angle logic needs care
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2 (after Task 4)
  - **Blocks**: Tasks 6, 7
  - **Blocked By**: Task 4

  **References**:

  **Pattern References**:
  - `src/main.cpp:138-139` - `motor1.move(cogging_compensation); motor1.loopFOC();` - Same move+loopFOC pattern used in FREE_SWING. POSITION uses the same pattern but with target_angle instead of compensation voltage.

  **API/Type References**:
  - SimpleFOC `motor1.move(target)` in angle mode: target is the desired angle in radians. The internal PID computes error = target - shaft_angle and outputs voltage.
  - SimpleFOC `motor1.shaft_angle` - cumulative angle (not wrapped to 0-2pi). This is why nearest-angle calculation is needed.
  - SimpleFOC `sensor1.getAngle()` - returns cumulative angle (same as shaft_angle after update)

  **WHY Each Reference Matters**:
  - Lines 138-139: Shows the move+loopFOC calling pattern that POSITION case follows
  - shaft_angle: Understanding that it is cumulative is critical for the nearest-angle target calculation

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with position mode body
    Tool: Bash (pio run)
    Preconditions: src/main.cpp POSITION case has move+loopFOC
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-5-build.txt

  Scenario: Position mode case is correctly implemented
    Tool: Read
    Preconditions: Task 5 changes applied
    Steps:
      1. Read the POSITION case in loop()
      2. Verify it calls motor1.move(target_angle)
      3. Verify it calls motor1.loopFOC() after move
      4. Verify the case ends with break
      5. Verify no trajectory tracking or motion profiling code exists
    Expected Result: Simple move+loopFOC+break in POSITION case
    Failure Indicators: Missing move or loopFOC, trajectory code, extra logic
    Evidence: .sisyphus/evidence/task-5-position.txt

  Scenario: Nearest-angle target calculation exists
    Tool: Grep
    Preconditions: Task 5 changes applied
    Steps:
      1. Grep for "candidate" or nearest-angle logic near T command parsing in processSerialInput
      2. Verify target_angle is computed as nearest equivalent to current shaft_angle
      3. Verify fmod and _2PI are used in the calculation
    Expected Result: Nearest-angle calculation prevents multi-turn unwinding
    Failure Indicators: Raw target assignment without nearest-angle, missing fmod/_2PI
    Evidence: .sisyphus/evidence/task-5-nearest-angle.txt
  ```

  **Commit**: YES
  - Message: `feat(motor): implement position mode hold`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

- [x] 6. Implement SUSTAIN mode

  **What to do**:
  - Fill in the FREE_SWING_SUSTAIN case in the loop() switch statement
  - This case does EVERYTHING the FREE_SWING case does (anticogging compensation), PLUS energy injection
  - To avoid code duplication, extract the shared anticogging logic into a helper function:
    ```
    float computeAnticogging(float current_angle, bool in_motion) {
      float map_position = current_angle / _2PI * MAP_SIZE;
      int index0 = (int)map_position % MAP_SIZE;
      int index1 = (index0 + 1) % MAP_SIZE;
      float fraction = map_position - (int)map_position;
      float map_value = calibration_map[index0] * (1.0f - fraction)
                      + calibration_map[index1] * fraction;
      if (in_motion && fabsf(map_value) >= cogging_deadband) {
        return -map_value * cogging_gain;
      }
      return 0.0f;
    }
    ```
  - Both FREE_SWING and FREE_SWING_SUSTAIN cases call this helper
  - For sustain injection, detect swing direction from the angle delta that motion gating already computes:
    - The `delta` variable (current_angle - gate_prev_angle, wrapped) from the motion gating code tells us the direction of movement
    - Store this delta as a `static float gate_last_delta = 0.0f;` so sustain can read it
    - When `in_motion` is true and sustain_enabled:
      ```
      float sustain_voltage = 0.0f;
      if (in_motion && sustain_enabled) {
        float direction = (gate_last_delta >= 0.0f) ? 1.0f : -1.0f;
        sustain_voltage = sustain_sign * direction * sustain_torque;
      }
      ```
    - Total voltage = anticogging_compensation + sustain_voltage
    - `motor1.move(total_voltage);`
  - The `sustain_sign` constant (+1 or -1) allows flipping the injection direction on hardware if the motor/belt convention is wrong. Default to +1.0f, user tunes on hardware.

  **Must NOT do**:
  - No automatic amplitude regulation (sustain is fixed magnitude)
  - No velocity-based direction detection (use angle delta only)
  - No adaptive sustain magnitude

  **Recommended Agent Profile**:
  - **Category**: `deep`
    - Reason: Involves extracting shared logic, direction detection from motion gating, and combining two voltage contributions
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2 (after Task 5)
  - **Blocks**: Task 7
  - **Blocked By**: Task 5

  **References**:

  **Pattern References**:
  - `src/main.cpp:90-124` (after Task 2 wrapping) - The anticogging compensation logic inside FREE_SWING case. This exact logic must be extracted into computeAnticogging() and shared with SUSTAIN case.
  - `src/main.cpp:108-117` - Motion gating code that computes `delta`. This delta's sign indicates swing direction. Store it for sustain to use.
  - `src/main.cpp:109` - `float delta = current_angle - gate_prev_angle;` - This is the raw angle change, wrapped across 0/2pi boundary. Its sign = movement direction.

  **API/Type References**:
  - `motor1.move(voltage)` in torque/voltage mode: voltage is directly applied as Vq. Sum of anticogging + sustain goes here.

  **WHY Each Reference Matters**:
  - Lines 90-124: The anticogging logic that must be shared, not duplicated
  - Lines 108-117: The motion gating delta that provides direction signal for sustain
  - Line 109: The specific delta calculation whose sign drives sustain direction

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with sustain mode
    Tool: Bash (pio run)
    Preconditions: src/main.cpp has FREE_SWING_SUSTAIN case with energy injection
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-6-build.txt

  Scenario: Sustain logic is correct
    Tool: Read + Grep
    Preconditions: Task 6 changes applied
    Steps:
      1. Read the FREE_SWING_SUSTAIN case in loop()
      2. Verify it computes anticogging compensation (via helper or inline)
      3. Verify it computes sustain_voltage based on gate_last_delta direction
      4. Verify total voltage = anticogging + sustain
      5. Verify motor1.move() receives the combined voltage
      6. Verify motor1.loopFOC() is called after move
      7. Grep for "sustain_sign" - used to flip injection direction
      8. Grep for "sustain_torque" - used as injection magnitude
      9. Verify sustain only active when in_motion is true
    Expected Result: Combined anticogging+sustain voltage, direction from angle delta, configurable sign, gated by motion
    Failure Indicators: Missing anticogging, velocity-based direction, ungated sustain, missing loopFOC
    Evidence: .sisyphus/evidence/task-6-sustain.txt

  Scenario: No code duplication between FREE_SWING and SUSTAIN
    Tool: Read
    Preconditions: Task 6 changes applied
    Steps:
      1. Read both FREE_SWING and FREE_SWING_SUSTAIN cases
      2. Verify anticogging logic is shared (via helper function), not copy-pasted
      3. Verify the helper function computeAnticogging() exists
    Expected Result: Shared helper function, no duplicated anticogging logic
    Failure Indicators: Copy-pasted anticogging code in both cases
    Evidence: .sisyphus/evidence/task-6-no-duplication.txt
  ```

  **Commit**: YES
  - Message: `feat(motor): implement sustain energy injection`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

- [x] 7. Add unified telemetry

  **What to do**:
  - Replace the existing `[COG]` debug output (currently in FREE_SWING case) with a unified telemetry system that works across all modes
  - Create a `printTelemetry()` function called at the end of loop() (after the switch), gated by debug toggle and interval:
    ```
    void printTelemetry(float angle, float comp, float sustain_v) {
      if (!debug_enabled) return;
      unsigned long now = millis();
      static unsigned long last_tel_ms = 0;
      if (now - last_tel_ms < cogging_debug_interval_ms) return;
      last_tel_ms = now;

      const char* mode_str = "UNKNOWN";
      switch (current_mode) {
        case POSITION:            mode_str = "POSITION"; break;
        case FREE_SWING:          mode_str = "FREE_SWING"; break;
        case FREE_SWING_SUSTAIN:  mode_str = "SUSTAIN"; break;
      }
      Serial.printf("[TEL] mode=%s angle=%.4f comp=%.4f sustain=%.4f\n",
                    mode_str, angle, comp, sustain_v);
    }
    ```
  - Rename `cogging_debug_enabled` to `debug_enabled` for clarity (it now covers all modes)
  - Call printTelemetry() from after the switch statement in loop(), passing the relevant values computed during the current mode's case
  - Store `comp` and `sustain_v` as local variables in loop() so they can be passed to telemetry:
    - Initialize both to 0.0f before the switch
    - FREE_SWING case sets `comp` from computeAnticogging()
    - FREE_SWING_SUSTAIN case sets both `comp` and `sustain_v`
    - POSITION case leaves both at 0.0f
  - Remove the old inline debug print from the FREE_SWING case body (replaced by unified telemetry)
  - In POSITION mode, also show target_angle:
    `Serial.printf("[TEL] mode=POSITION angle=%.4f target=%.4f\n", angle, target_angle);`
    Use a slightly different format for position to include target.

  **Must NOT do**:
  - Do not add telemetry to serial commands (those already print [MODE] and [TARGET] tags)
  - Do not add WiFi/network telemetry
  - Do not add logging to files

  **Recommended Agent Profile**:
  - **Category**: `quick`
    - Reason: Consolidating existing debug output into a unified function
  - **Skills**: []

  **Parallelization**:
  - **Can Run In Parallel**: NO
  - **Parallel Group**: Wave 2 (after Task 6)
  - **Blocks**: F1-F4
  - **Blocked By**: Task 6

  **References**:

  **Pattern References**:
  - `src/main.cpp:126-136` - Existing `[COG]` debug output code that gets replaced. Shows the current interval-gated printf pattern.
  - `src/main.cpp:27-32` - Existing debug constants (cogging_debug_enabled, cogging_debug_interval_ms). The bool was made mutable in Task 3. Rename to debug_enabled.

  **WHY Each Reference Matters**:
  - Lines 126-136: This is the code being replaced with unified telemetry
  - Lines 27-32: The debug constants that need renaming/updating

  **QA Scenarios (MANDATORY):**

  ```
  Scenario: Build succeeds with unified telemetry
    Tool: Bash (pio run)
    Preconditions: src/main.cpp has printTelemetry() function
    Steps:
      1. Run: pio run
      2. Check output for "SUCCESS" and zero errors
    Expected Result: Compilation succeeds with 0 errors, 0 warnings
    Failure Indicators: Any "error:" or "warning:" in pio output
    Evidence: .sisyphus/evidence/task-7-build.txt

  Scenario: Telemetry format is correct
    Tool: Grep
    Preconditions: Task 7 changes applied
    Steps:
      1. Grep for "[TEL]" in Serial.printf strings - must exist
      2. Grep for "mode=" in telemetry format - must show current mode
      3. Grep for "angle=" in telemetry format - must show current angle
      4. Grep for "comp=" in telemetry format - must show compensation
      5. Grep for "sustain=" in telemetry format - must show sustain voltage
      6. Grep for old "[COG]" format - must NOT exist (replaced)
    Expected Result: Unified [TEL] format with mode/angle/comp/sustain, old [COG] removed
    Failure Indicators: Old [COG] still present, missing fields in [TEL], format mismatch
    Evidence: .sisyphus/evidence/task-7-format.txt

  Scenario: Telemetry is called from loop after switch
    Tool: Read
    Preconditions: Task 7 changes applied
    Steps:
      1. Read loop() function
      2. Verify printTelemetry() is called AFTER the switch statement (not inside a case)
      3. Verify comp and sustain_v variables are declared before switch and set inside cases
      4. Verify the old inline debug code is removed from FREE_SWING case
    Expected Result: Telemetry called once after switch, variables flow from cases to telemetry
    Failure Indicators: Telemetry inside a case (not unified), old debug code still present
    Evidence: .sisyphus/evidence/task-7-unified.txt
  ```

  **Commit**: YES
  - Message: `feat(telemetry): add unified mode-aware telemetry`
  - Files: `src/main.cpp`
  - Pre-commit: `pio run`

---

## Final Verification Wave

> 4 review agents run in PARALLEL. ALL must APPROVE. Present consolidated results to user and get explicit okay before completing.
> Do NOT auto-proceed after verification. Wait for user explicit approval before marking work complete.

- [ ] F1. **Plan Compliance Audit** - `oracle`
  Read the plan end-to-end. For each Must Have: verify implementation exists (read file, grep for enum/functions/serial handling). For each Must NOT Have: search codebase for forbidden patterns (trajectory tracking, Commander class, direct motor1.controller assignment in loop, WiFi/BLE includes). Check evidence files exist in .sisyphus/evidence/. Compare deliverables against plan.
  Output: `Must Have [N/N] | Must NOT Have [N/N] | Tasks [N/N] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Code Quality Review** - `unspecified-high`
  Run `pio run` (if available). Review all changed code in main.cpp for: unsafe casts, empty catches, dangling pointers, buffer overflows in serial parsing, unused variables, commented-out code beyond the existing M2 pins. Check AI slop: excessive comments, over-abstraction, generic variable names. Verify all constexpr tuning parameters are grouped at file top. Verify no raw `motor1.controller =` assignments outside transition functions and calibrateCogging().
  Output: `Build [PASS/FAIL] | Style [N clean/N issues] | Safety [N issues] | VERDICT`

- [ ] F3. **Build Verification + Code Pattern QA** - `unspecified-high`
  Attempt `pio run` compilation. If pio unavailable, do static code review: verify all SimpleFOC API calls match v2.4.0 signatures, verify enum values match switch cases (no missing cases), verify serial parsing handles edge cases (empty input, garbage input, numbers out of range). Verify all telemetry format strings are parseable. Check that calibration blocking loop flushes serial buffer on exit.
  Output: `Build [PASS/FAIL/DEFERRED] | API Correctness [N/N] | Edge Cases [N/N] | VERDICT`

- [ ] F4. **Scope Fidelity Check** - `deep`
  For each task: read What to do, read actual code in main.cpp. Verify 1:1 - everything in spec was built (no missing), nothing beyond spec was built (no creep). Check Must NOT do compliance. Verify no trajectory tracking, no auto mode switching, no BLE, no Commander, no file splitting. Verify existing anticogging compensation logic is IDENTICAL to original (compare the FREE_SWING case body against original loop body, accounting for extraction into helper function).
  Output: `Tasks [N/N compliant] | Creep [CLEAN/N issues] | Anticogging Preserved [YES/NO] | VERDICT`

---

## Commit Strategy

| # | Commit Message | Files | Pre-commit Check |
|---|---------------|-------|-----------------|
| 1 | `refactor(motor): add state machine enum and mode variable` | src/main.cpp | pio run |
| 2 | `refactor(motor): wrap loop body in FREE_SWING state` | src/main.cpp | pio run |
| 3 | `feat(serial): add command parser for mode switching` | src/main.cpp | pio run |
| 4 | `feat(motor): implement safe mode transitions` | src/main.cpp | pio run |
| 5 | `feat(motor): implement position mode hold` | src/main.cpp | pio run |
| 6 | `feat(motor): implement sustain energy injection` | src/main.cpp | pio run |
| 7 | `feat(telemetry): add unified mode-aware telemetry` | src/main.cpp | pio run |

---

## Success Criteria

### Verification Commands
```bash
pio run  # Expected: SUCCESS, zero errors, zero warnings
```

### Final Checklist
- [ ] State machine with 3 modes present
- [ ] Serial commands P, F, S, T, C, D all functional
- [ ] Safe mode transitions (disable/set controller/set voltage_limit/enable)
- [ ] Position mode holds angle with nearest-angle clamping
- [ ] Free-swing mode applies anticogging compensation (unchanged from original)
- [ ] Sustain mode injects direction-aware energy (anticogging + sustain voltage)
- [ ] Telemetry shows mode, angle, compensation, sustain state via unified [TEL] format
- [ ] Boot defaults to FREE_SWING
- [ ] No forbidden patterns (trajectory, auto-mode, BLE, Commander, etc.)
- [ ] All Must NOT Have guardrails respected
