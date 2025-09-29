# Requirements Document

## Introduction

This feature evolves the existing animatronic skull proof-of-concept into a comprehensive animation sequencing system. The current system controls three servo motors (pan, nod, jaw) and LCD eyes with basic keyframe sequences. The enhanced system will provide sophisticated eye tracking control, scriptable animation sequences, and eventually dynamic movement capabilities that can respond to external inputs or generate procedural animations.

The system will maintain compatibility with the existing hardware setup (Pololu Maestro servo controller, TFT displays for eyes, ESP32 microcontroller) while adding layers of abstraction that enable more complex and interesting animation behaviors.

# Overall Guidelines
This is a simple home weekend project; AVOID too many tests, abstractions, and other forms of unnecessary complexity. The overall project should still be a few hundred lines in total.

## Requirements

### Requirement 1
**User Story:** As a developer, I want the code to be clean and maintainable. There are too many constants spread around the source code and we need to keep them clean in a separate config file.

### Requirement 2

**User Story:** As an animatronic developer, in addition to the existing "scripted" mode, I want the system to support a "dynamic" mode that allows for more lifelike sequences. 

#### Acceptance Criteria

0. A configuration value in the setup() function will specify which mode to use
1. WHEN in scripted mode THEN the system SHALL execute sequences with precise timing and repeatability following the Keyframe sequence.
2. WHEN in dynamic mode THEN the system SHALL generate movements based on configurable parameters and algorithms. Instead of specifying specific positions, the dynamic sequence will procedurally determine appropriate movements.

### Requirement 3

**User Story:** As an animatronic operator, I can use the BluetoothSerial package to send commands from a connected device.

### Requirement 4

**User Story:** As an animatronic developer, I want to synchronize the jaw movement to spoken sound. Initially this will be by specifying a very simplistic phoneme library that can be sequenced in either scripted mode or dynamic mode.

