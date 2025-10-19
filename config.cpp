#include "config.h"
#include <pgmspace.h>

// =============================================================================
// GLOBAL VARIABLE DEFINITIONS
// =============================================================================

const ServoMotionConfig PROGMEM SERVO_MOTION_CONFIGS[] = {
    {SKULL_PAN_CHANNEL, 60, 30},   // Pan: moderate speed and acceleration
    {SKULL_NOD_CHANNEL, 50, 25},   // Nod: slightly slower for smoothness
    {SKULL_JAW_CHANNEL, 0, 100}    // Jaw: unlimited speed, high acceleration
};

const ServoRange PROGMEM SERVO_RANGES[] = {
    {SKULL_PAN_CHANNEL, PAN_LEFT, PAN_RIGHT, PAN_CENTER},
    {SKULL_NOD_CHANNEL, NOD_UP, NOD_DOWN, NOD_CENTER},
    {SKULL_JAW_CHANNEL, JAW_CLOSED, JAW_OPEN, JAW_CLOSED}
};

const DynamicModeConfig PROGMEM DEFAULT_DYNAMIC_CONFIG = {
    1000,  // minMovementInterval: 1 second
    4000,  // maxMovementInterval: 4 seconds
    0.7f,  // movementIntensity: 70% of full range
    500,   // minHoldDuration: 0.5 seconds
    2000   // maxHoldDuration: 2 seconds
};

const DynamicModeConfig PROGMEM TALKING_DYNAMIC_CONFIG = {
    2000,  // minMovementInterval: 3 seconds
    5000,  // maxMovementInterval: 8 seconds
    0.3f,  // movementIntensity: 20% of full range
    500,   // minHoldDuration: 0.5 seconds
    1500   // maxHoldDuration: 2 seconds
};

// =============================================================================
// VALIDATION FUNCTION IMPLEMENTATIONS
// =============================================================================

bool validateServoPosition(uint8_t channel, uint16_t position) {
    const ServoRange* range = getServoRange(channel);
    if (range == nullptr) {
        return false; // Unknown channel
    }

    return (position >= range->min && position <= range->max);
}

bool validateEyePosition(int16_t h_offset, int16_t v_offset) {
    // Define reasonable bounds for eye movement
    const int16_t MAX_H_OFFSET = 60;  // Maximum horizontal offset
    const int16_t MAX_V_OFFSET = 30;  // Maximum vertical offset

    return (h_offset >= -MAX_H_OFFSET && h_offset <= MAX_H_OFFSET &&
            v_offset >= -MAX_V_OFFSET && v_offset <= MAX_V_OFFSET);
}

bool validateTiming(uint32_t duration_ms) {
    // Define reasonable timing bounds
    const uint32_t MIN_DURATION_MS = 10;    // Minimum 10ms
    const uint32_t MAX_DURATION_MS = 30000; // Maximum 30 seconds

    return (duration_ms >= MIN_DURATION_MS && duration_ms <= MAX_DURATION_MS);
}

const ServoRange* getServoRange(uint8_t channel) {
    for (int i = 0; i < NUM_SERVOS; i++) {
        uint8_t channel_from_progmem = pgm_read_byte(&SERVO_RANGES[i].channel);
        if (channel_from_progmem == channel) {
            return &SERVO_RANGES[i]; // Return pointer to PROGMEM data
        }
    }
    return nullptr; // Channel not found
}

const ServoMotionConfig* getServoMotionConfig(uint8_t channel) {
    for (int i = 0; i < NUM_SERVO_MOTION_CONFIGS; i++) {
        uint8_t channel_from_progmem = pgm_read_byte(&SERVO_MOTION_CONFIGS[i].channel);
        if (channel_from_progmem == channel) {
            return &SERVO_MOTION_CONFIGS[i]; // Return pointer to PROGMEM data
        }
    }
    return nullptr; // Channel not found
}