#include "config.h"

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
        if (SERVO_RANGES[i].channel == channel) {
            return &SERVO_RANGES[i];
        }
    }
    return nullptr; // Channel not found
}

const ServoMotionConfig* getServoMotionConfig(uint8_t channel) {
    for (int i = 0; i < NUM_SERVO_MOTION_CONFIGS; i++) {
        if (SERVO_MOTION_CONFIGS[i].channel == channel) {
            return &SERVO_MOTION_CONFIGS[i];
        }
    }
    return nullptr; // Channel not found
}