#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// =============================================================================
// HARDWARE CONFIGURATION
// =============================================================================

// Maestro servo controller pins
static const int MAESTRO_TX_PIN = 21;

// Servo channel assignments
static const int SKULL_PAN_CHANNEL = 0;
static const int SKULL_NOD_CHANNEL = 1;
static const int SKULL_JAW_CHANNEL = 2;

// =============================================================================
// SERVO POSITION CONSTANTS
// =============================================================================

// Pan servo positions (horizontal head movement)
#define PAN_LEFT 4416
#define PAN_CENTER 6000
#define PAN_RIGHT 7232

// Nod servo positions (vertical head movement)
#define NOD_DOWN 4992
#define NOD_CENTER 4600
#define NOD_UP 4224

// Jaw servo positions
#define JAW_CLOSED 5888
#define JAW_OPEN 6528

// =============================================================================
// EYE CONFIGURATION
// =============================================================================

// Eye geometry constants
const int EYE_CENTER_X = 120;
const int EYE_CENTER_Y = 120;
const int PUPIL_RADIUS = 20;

// Eye position offsets
#define EYE_H_LEFT -40
#define EYE_H_CENTER 0
#define EYE_H_RIGHT 40
#define EYE_V_UP -30
#define EYE_V_CENTER 0
#define EYE_V_DOWN 30

// =============================================================================
// TIMING CONFIGURATION
// =============================================================================

// Blink timing constants
const int BLINK_INTERVAL_MIN_MS = 1000;
const int BLINK_INTERVAL_MAX_MS = 5000;

// Servo motion parameters
struct ServoMotionConfig {
    uint8_t channel;
    uint16_t speed;        // 0 = unlimited, 1 = 0.25 us / 10 ms
    uint16_t acceleration; // 0 = unlimited, 1 = 0.25 us / 10 ms / 80 ms
};

extern const ServoMotionConfig SERVO_MOTION_CONFIGS[];
#define NUM_SERVO_MOTION_CONFIGS 3

// =============================================================================
// DISPLAY CONFIGURATION
// =============================================================================

// Screen resolution and rotation
#define TFT_HOR_RES   240
#define TFT_VER_RES   240
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0

// LVGL buffer size (1/10 screen size usually works well)
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))

// =============================================================================
// SERVO RANGE CONFIGURATION
// =============================================================================

// Data structure to store the min/max extents for each servo
struct ServoRange {
    uint8_t channel;
    uint16_t min;
    uint16_t max;
    uint16_t home;
};

extern const ServoRange SERVO_RANGES[];
#define NUM_SERVOS 3

// =============================================================================
// OPERATION MODE CONFIGURATION
// =============================================================================

// Operation modes for the animatronic system
enum class OperationMode {
    SCRIPTED,  // Execute predefined sequences with precise timing
    DYNAMIC,   // Generate procedural movements with configurable parameters
    TALKING    // Simulate talking with jaw movements
};

// Dynamic mode configuration parameters
struct DynamicModeConfig {
    uint32_t minMovementInterval;    // Minimum time between movements (ms)
    uint32_t maxMovementInterval;    // Maximum time between movements (ms)
    float movementIntensity;         // 0.0 to 1.0 - how far from center to move
    uint32_t minHoldDuration;        // Minimum time to hold a position (ms)
    uint32_t maxHoldDuration;        // Maximum time to hold a position (ms)
};

// Default dynamic mode configuration
extern const DynamicModeConfig DEFAULT_DYNAMIC_CONFIG;

// =============================================================================
// TALKING MODE CONFIGURATION
// =============================================================================

// Timing for simulated speech patterns
const uint32_t TALK_SEGMENT_DURATION_MIN_MS = 200;  // Min duration of a "word"
const uint32_t TALK_SEGMENT_DURATION_MAX_MS = 1500; // Max duration of a "sentence"
const uint32_t TALK_PAUSE_DURATION_MIN_MS = 100;    // Min pause between "words"
const uint32_t TALK_PAUSE_DURATION_MAX_MS = 500;    // Max pause between "sentences"

// =============================================================================
// BLUETOOTH CONFIGURATION
// =============================================================================

// Bluetooth device name
#define BT_DEVICE_NAME "IndianaBones"

// Command parsing constants
const int MAX_COMMAND_LENGTH = 64;
const char COMMAND_DELIMITER = '\n';

// =============================================================================
// ANIMATION CONFIGURATION
// =============================================================================

// Default animation durations (milliseconds)
const uint32_t DEFAULT_EYE_ANIMATION_DURATION = 500;
const uint32_t DEFAULT_BLINK_CLOSE_DURATION = 150;
const uint32_t DEFAULT_BLINK_PAUSE_DURATION = 100;
const uint32_t DEFAULT_BLINK_OPEN_DURATION = 150;

// Eyelid dimensions
const int EYELID_HEIGHT = 120;
const int EYELID_WIDTH = 240;

// =============================================================================
// VALIDATION FUNCTIONS
// =============================================================================

/**
 * Validates that a servo position is within the configured range for the given channel
 * @param channel The servo channel to validate
 * @param position The position to validate
 * @return true if position is valid, false otherwise
 */
bool validateServoPosition(uint8_t channel, uint16_t position);

/**
 * Validates that eye position offsets are within reasonable bounds
 * @param h_offset Horizontal offset from center
 * @param v_offset Vertical offset from center
 * @return true if offsets are valid, false otherwise
 */
bool validateEyePosition(int16_t h_offset, int16_t v_offset);

/**
 * Validates that timing values are reasonable
 * @param duration_ms Duration in milliseconds
 * @return true if duration is valid, false otherwise
 */
bool validateTiming(uint32_t duration_ms);

/**
 * Gets the servo range configuration for a given channel
 * @param channel The servo channel
 * @return Pointer to ServoRange struct, or nullptr if channel not found
 */
const ServoRange* getServoRange(uint8_t channel);

/**
 * Gets the servo motion configuration for a given channel
 * @param channel The servo channel
 * @return Pointer to ServoMotionConfig struct, or nullptr if channel not found
 */
const ServoMotionConfig* getServoMotionConfig(uint8_t channel);

#endif // CONFIG_H