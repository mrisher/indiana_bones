# Implementation Plan

- [x] 1. Extract configuration constants into separate config system

  - Create config.h file to centralize servo limits, pin definitions, and timing constants from tft_test.ino
  - Move hardcoded servo ranges, eye positions, and pin definitions into structured configuration
  - Add simple configuration validation functions
  - _Requirements: 1_

- [x] 2. Implement mode selection system

  - [x] 2.1 Add mode configuration and selection

    - Create enum for operation modes (SCRIPTED, DYNAMIC)
    - Add mode selection variable in setup() function
    - Implement mode-specific initialization logic
    - _Requirements: 2.0_

  - [x] 2.2 Implement dynamic mode animation engine
    - Create simple procedural movement generation that varies timing and positions randomly within servo ranges
    - Add configurable parameters for dynamic behavior (movement intensity, timing variations)
    - Implement dynamic sequence generation that creates keyframes procedurally instead of using fixed sequence array
    - _Requirements: 2.1, 2.2_

- [x] 3. Add BluetoothSerial command interface

  - [x] 3.1 Implement Bluetooth communication setup

    - Add BluetoothSerial library integration to existing code
    - Create simple command parsing system for basic commands (start, stop, pause, resume, mode switch)
    - Implement command processing in main loop
    - _Requirements: 3_

  - [x] 3.2 Add real-time control commands
    - Implement direct position control commands for individual servos and eye positions
    - Add sequence selection commands to switch between different predefined sequences
    - Create basic status reporting back to connected device
    - _Requirements: 3_

- [ ] 4. Create phoneme library for jaw synchronization

  - [ ] 4.1 Implement basic phoneme definitions

    - Create simple phoneme data structure with jaw positions for basic vowels (A, E, I, O, U)
    - Define phoneme-to-jaw-position mapping using existing JAW_OPEN/JAW_CLOSED constants
    - Add function to convert phoneme sequences to jaw keyframes
    - _Requirements: 4_

  - [ ] 4.2 Integrate phoneme sequencing
    - Add phoneme sequence support to existing keyframe system
    - Implement simple phoneme timing that can be integrated into both scripted and dynamic modes
    - Create basic phoneme sequence parsing from simple phoneme code strings
    - _Requirements: 4_
