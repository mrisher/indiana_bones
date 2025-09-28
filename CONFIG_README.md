# Configuration System

This document describes the configuration system that centralizes all constants and settings for the animatronic sequencer.

## Files

- `config.h` - Header file containing all configuration constants and function declarations
- `config.cpp` - Implementation file containing validation functions
- `tft_test.ino` - Main Arduino sketch that uses the configuration system

## Configuration Categories

### Hardware Configuration
- `MAESTRO_TX_PIN` - Pin for Maestro servo controller communication
- `SKULL_PAN_CHANNEL`, `SKULL_NOD_CHANNEL`, `SKULL_JAW_CHANNEL` - Servo channel assignments

### Servo Position Constants
- Pan positions: `PAN_LEFT`, `PAN_CENTER`, `PAN_RIGHT`
- Nod positions: `NOD_DOWN`, `NOD_CENTER`, `NOD_UP`
- Jaw positions: `JAW_CLOSED`, `JAW_OPEN`

### Eye Configuration
- `EYE_CENTER_X`, `EYE_CENTER_Y` - Eye center coordinates
- `PUPIL_RADIUS` - Pupil size
- Eye position offsets: `EYE_H_LEFT/CENTER/RIGHT`, `EYE_V_UP/CENTER/DOWN`

### Timing Configuration
- `BLINK_INTERVAL_MIN_MS`, `BLINK_INTERVAL_MAX_MS` - Automatic blink timing
- `DEFAULT_EYE_ANIMATION_DURATION` - Default eye movement duration
- Blink animation durations: `DEFAULT_BLINK_CLOSE/PAUSE/OPEN_DURATION`

### Display Configuration
- `TFT_HOR_RES`, `TFT_VER_RES` - Screen resolution
- `TFT_ROTATION` - Display rotation
- `DRAW_BUF_SIZE` - LVGL buffer size

## Data Structures

### ServoRange
Defines the operational range for each servo:
- `channel` - Servo channel number
- `min` - Minimum position value
- `max` - Maximum position value  
- `home` - Default/home position

### ServoMotionConfig
Defines motion parameters for each servo:
- `channel` - Servo channel number
- `speed` - Movement speed (0 = unlimited)
- `acceleration` - Acceleration limit

## Validation Functions

### `validateServoPosition(channel, position)`
Validates that a servo position is within the configured range for the given channel.

### `validateEyePosition(h_offset, v_offset)`
Validates that eye position offsets are within reasonable bounds.

### `validateTiming(duration_ms)`
Validates that timing values are reasonable (between 10ms and 30 seconds).

### `getServoRange(channel)`
Returns the ServoRange configuration for a given channel.

### `getServoMotionConfig(channel)`
Returns the ServoMotionConfig for a given channel.

## Usage Example

```cpp
#include "config.h"

// Get servo configuration
const ServoRange* panRange = getServoRange(SKULL_PAN_CHANNEL);
if (panRange != nullptr) {
    // Use panRange->min, panRange->max, etc.
}

// Validate before setting position
if (validateServoPosition(SKULL_PAN_CHANNEL, PAN_LEFT)) {
    maestro.setTarget(SKULL_PAN_CHANNEL, PAN_LEFT);
}
```

## Benefits

1. **Centralized Configuration** - All constants in one place
2. **Type Safety** - Proper data types for all values
3. **Validation** - Built-in validation prevents invalid operations
4. **Maintainability** - Easy to modify settings without hunting through code
5. **Documentation** - Clear organization and comments