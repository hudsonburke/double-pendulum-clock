# Task 6: FREE_SWING_SUSTAIN + computeAnticogging refactor

## What was done
- Added `float computeAnticogging(float current_angle, bool in_motion)` helper at line ~197 (before `loop()`)
- Added forward declaration `float computeAnticogging(float, bool);` at line 57 (with other forward decls)
- Added `static float gate_last_delta = 0.0f;` to statics block in `loop()`
- Refactored `FREE_SWING` case: removed inline map lookup (7 lines), now calls `computeAnticogging(current_angle, in_motion)`; added `gate_last_delta = delta;` in gating block
- Implemented `FREE_SWING_SUSTAIN` case with full motion gating + `computeAnticogging()` + `sustain_voltage = sustain_sign * direction * sustain_torque` (direction from `gate_last_delta` sign)

## Key facts
- `map_position` appears exactly 4 times, ALL inside `computeAnticogging` — no duplication
- `computeAnticogging` referenced 4 times: forward decl, definition, FREE_SWING call, FREE_SWING_SUSTAIN call
- `pio run` → SUCCESS, zero errors, 11.6% flash used
- Commit: eb58c7a "feat(motor): implement sustain energy injection"
