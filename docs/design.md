# Design Documentation

This is a slightly odd design problem because we have largely settled on the form of the solution (a double pendulum) and the main design challenges are in the control system and user interaction. However, we can still apply design thinking principles to ensure we are creating a solution that meets users'/our needs.

## Needs

- Shows time using a double pendulum with 2 arms of a fixed length
- Represents chaotic motion visually
- Allows for user interaction
- Safe enough for public display
- Minimizes maintenance

## How Might We

- Accurately control the motion of the pendulum arms to display the correct time?
- Allow for both controlled and chaotic motion in the same system?
- Allow users to influence the pendulum motion without damaging it or causing safety issues?

## Functional Requirements

- Displays current time
- Moves to controlled position
- Behaves as "natural" chaotic motion when not controlled
- Detects user presence to influence motion

## Design Requirements

- Fits within ____
- Maximum inaccuracy of ____ seconds in time display
- Moves within ____ mm of defined position within ____ seconds
- Enters into chaotic motion within ____ seconds of trigger
- Exits chaotic motion and returns to controlled position within ____ seconds of trigger
- Senses user presence within ____ distance
- Reacts to user presence within ____ seconds

## Concept Generation

### Control Mechanism

- Background xy axes for positioning
- Magnetic coupling for controlling the pendulum arms without physical contact
- Central motors with belts and pulleys to directly control the arms
- Clutch mechanism to decouple motors from arms for chaotic motion

### Motors

- Brushless
- Servos
- Brushed DC
- Stepper

### User Interaction

- Motion sensor
- mmWave radar
- Button

### Real Time Clock (RTC)

- GPS-based
- WiFi or Ethernet connection
- RTC module w/ battery backup and manual time setting like DS3234
- [Research Doc](https://docs.google.com/document/d/1Ugsa2n1ORUJYejVRIX3g0Py19Keq57nNqj4PwvI38Ho/edit?usp=sharing)

### Microcontroller

- Propeller 2
- Teensy 4.1
- ESP32

## Concept Selection

### Control System

- Clutch mechanism for switching between controlled and chaotic motion
- Fallback to magnetic coupling if clutch proves infeasible
- Fallback to xy axes if central motors prove infeasible
