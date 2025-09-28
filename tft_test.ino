/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */

#include <lvgl.h>

#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
#endif

#include <HardwareSerial.h>
#include <PololuMaestro.h>
#include "config.h"

HardwareSerial maestroSerial(2);
MiniMaestro maestro(maestroSerial);

// Eye animation globals
static lv_obj_t * eye;
static lv_obj_t * pupil;
static int16_t current_eye_h = 0;
static int16_t current_eye_v = 0;

// Automatic blink timer
unsigned long last_blink_time = 0;
unsigned long next_blink_interval = 0;

// Operation mode configuration
OperationMode currentMode = OperationMode::SCRIPTED; // Default to scripted mode

// Dynamic mode state variables
DynamicModeConfig dynamicConfig = DEFAULT_DYNAMIC_CONFIG;
unsigned long lastDynamicMovement = 0;
unsigned long nextDynamicMovement = 0;
bool dynamicModeInitialized = false;

// All constants now defined in config.h

// Servo range configuration now in config.h

// 2) create a simple sequence where I can specify the positions and start times of each "keyframe"
struct Keyframe {
  uint32_t startTime;
  uint16_t positions[NUM_SERVOS];
  int16_t eye_h_pos;
  int16_t eye_v_pos;
};

Keyframe sequence[] = {
  {   0, {PAN_CENTER, NOD_CENTER, JAW_CLOSED}, EYE_H_CENTER, EYE_V_CENTER }, // center, center, jaw closed
  { 500, {PAN_LEFT,   NOD_CENTER, JAW_CLOSED}, EYE_H_LEFT,   EYE_V_CENTER }, // look left
  {1500, {PAN_RIGHT,  NOD_CENTER, JAW_CLOSED}, EYE_H_RIGHT,  EYE_V_CENTER }, // look right
  {2500, {PAN_CENTER, NOD_CENTER, JAW_CLOSED}, EYE_H_CENTER, EYE_V_CENTER }, // center
  {3000, {PAN_CENTER, NOD_DOWN,   JAW_CLOSED}, EYE_H_CENTER, EYE_V_DOWN   }, // look down
  {4000, {PAN_CENTER, NOD_UP,     JAW_CLOSED}, EYE_H_CENTER, EYE_V_UP     }, // look up
  {5000, {PAN_CENTER, NOD_CENTER, JAW_CLOSED}, EYE_H_CENTER, EYE_V_CENTER }, // center
  {5500, {PAN_CENTER, NOD_CENTER, JAW_OPEN}  , EYE_H_CENTER, EYE_V_CENTER }, // open jaw
  {6000, {PAN_CENTER, NOD_CENTER, JAW_CLOSED}, EYE_H_CENTER, EYE_V_CENTER }, // close jaw
  {6500, {PAN_CENTER, NOD_CENTER, JAW_OPEN}  , EYE_H_CENTER, EYE_V_CENTER }, // open jaw
  {7000, {PAN_CENTER, NOD_CENTER, JAW_CLOSED}, EYE_H_CENTER, EYE_V_CENTER }  // close jaw
};
const int NUM_KEYFRAMES = sizeof(sequence) / sizeof(Keyframe);
// The total duration of the sequence is the start time of the last keyframe.
const uint32_t sequence_duration = sequence[NUM_KEYFRAMES - 1].startTime;

// for tracking playback
unsigned long sequenceStartTime = 0;
int nextKeyframeIndex = 0;

// Display configuration now in config.h
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

#if LV_USE_LOG != 0
void my_print( lv_log_level_t level, const char * buf )
{
    LV_UNUSED(level);
    Serial.println(buf);
    Serial.flush();
}
#endif

/* Global */
static lv_grad_dsc_t SCLERA_GRADIENT;

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush( lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    /*Copy `px map` to the `area`*/

    /*For example ("my_..." functions needs to be implemented by you)
    uint32_t w = lv_area_get_width(area);
    uint32_t h = lv_area_get_height(area);

    my_set_window(area->x1, area->y1, w, h);
    my_draw_bitmaps(px_map, w * h);
     */

    /*Call it to tell LVGL you are ready*/
    lv_display_flush_ready(disp);
}

/*Read the touchpad*/
void my_touchpad_read( lv_indev_t * indev, lv_indev_data_t * data )
{
    /*For example  ("my_..." functions needs to be implemented by you)
    int32_t x, y;
    bool touched = my_get_touch( &x, &y );

    if(!touched) {
        data->state = LV_INDEV_STATE_RELEASED;
    } else {
        data->state = LV_INDEV_STATE_PRESSED;

        data->point.x = x;
        data->point.y = y;
    }
     */
}

// Animation state
static int16_t anim_start_h, anim_end_h, anim_start_v, anim_end_v;

static void anim_eye_callback(void * obj, int32_t v)
{
    // `v` is the eased progress value from the animation path, in range [0, 256].
    // We use it to interpolate the horizontal and vertical eye positions.
    int16_t new_h = anim_start_h + (int32_t)(anim_end_h - anim_start_h) * v / 256;
    int16_t new_v = anim_start_v + (int32_t)(anim_end_v - anim_start_v) * v / 256;

    // Pupil update
    lv_obj_t * pupil = lv_obj_get_child((lv_obj_t *) obj, NULL);
    int16_t pupil_x = EYE_CENTER_X - PUPIL_RADIUS + new_h;
    int16_t pupil_y = EYE_CENTER_Y - PUPIL_RADIUS + new_v;
    lv_obj_set_pos(pupil, pupil_x, pupil_y);

    // Sclera update
    const int inner_radius = 30;
    int16_t sclera_x = EYE_CENTER_X + new_h;
    int16_t sclera_y = EYE_CENTER_Y + new_v;
    SCLERA_GRADIENT.params.radial.focal.x = sclera_x;
    SCLERA_GRADIENT.params.radial.focal.y = sclera_y;
    SCLERA_GRADIENT.params.radial.focal_extent.x = sclera_x + inner_radius;
    SCLERA_GRADIENT.params.radial.focal_extent.y = sclera_y + inner_radius;
    lv_obj_invalidate((lv_obj_t *)obj);
}

void animate_eye_to(int16_t target_h, int16_t target_v, uint32_t duration) {
    // Set up the start and end points for our custom animation callback
    anim_start_h = current_eye_h;
    anim_end_h = target_h;
    anim_start_v = current_eye_v;
    anim_end_v = target_v;

    // A single animation to drive the callback
    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, eye);

    // Animate a "progress" value from 0 to 256.
    // The callback will use this to interpolate the actual coordinates.
    lv_anim_set_values(&anim, 0, 256);
    lv_anim_set_duration(&anim, duration);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&anim, anim_eye_callback);
    lv_anim_start(&anim);

    // Update current position immediately for the next keyframe calculation
    current_eye_h = target_h;
    current_eye_v = target_v;
}

void draw_outer_eyeball(int center_x, int center_y, int offset_x, int offset_y)
{
    // Gradient Docs: https://docs.lvgl.io/9.3/details/main-modules/draw/draw_descriptors.html#radial-gradients and
    // https://github.com/lvgl/lvgl/blob/master/examples/SCLERA_GRADIENT/lv_example_grad_3.c
    const int outer_radius = 80;
    const int inner_radius = 30;

    static const lv_color_t grad_colors[2] = {
        LV_COLOR_MAKE(0xCC, 0x00, 0x66),
        LV_COLOR_MAKE(0xFF, 0x00, 0x7F),
    };

    static const lv_opa_t grad_opa[2] = {
        LV_OPA_100,
        LV_OPA_0,
    };

    static lv_style_t style;
    lv_style_init(&style);

    lv_grad_init_stops(&SCLERA_GRADIENT, grad_colors, grad_opa, NULL, sizeof(grad_colors) / sizeof(lv_color_t));

    /*Init a radial gradient where the center is at 0;0
     *and the edge of the circle is at 100;100.
     *Try LV_GRAD_EXTEND_REFLECT and LV_GRAD_EXTEND_REPEAT too. */
    lv_grad_radial_init(&SCLERA_GRADIENT, center_x, center_y, center_x + outer_radius, center_y + outer_radius, LV_GRAD_EXTEND_PAD);

    /*The gradient will be calculated between the focal point's circle and the
     *edge of the circle. If the center of the focal point and the
     *center of the main circle is the same, the gradient will spread
     *evenly in all directions. The focal point should be inside the
     *main circle.*/
    lv_grad_radial_set_focal(&SCLERA_GRADIENT, center_x + offset_x, center_y + offset_y, inner_radius);

    /*Set the widget containing the gradient*/
    lv_style_set_bg_grad(&style, &SCLERA_GRADIENT);
    lv_style_set_border_width(&style, 0);

    /*Create an object with the new style*/
    eye = lv_obj_create(lv_screen_active());
    lv_obj_add_style(eye, &style, 0);
    lv_obj_set_size(eye, lv_pct(100), lv_pct(100));
    lv_obj_center(eye);

    // pupil
    pupil = lv_obj_create(eye);
    lv_obj_set_size(pupil, PUPIL_RADIUS, PUPIL_RADIUS);
    // set_pos is upper-left corner
    lv_obj_set_pos(pupil, center_x - PUPIL_RADIUS + offset_x, center_y - PUPIL_RADIUS + offset_y);
    lv_obj_set_style_bg_color(pupil, lv_color_hex(0xffcc00), 0);
    lv_obj_set_style_border_width(pupil, 0, 0);
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
}

// Eyelid animation globals
static lv_obj_t * eyelid_top;
static lv_obj_t * eyelid_bottom;

void trigger_blink() {
    lv_anim_t anim_blink_top;
    lv_anim_init(&anim_blink_top);
    lv_anim_set_var(&anim_blink_top, eyelid_top);
    lv_anim_set_values(&anim_blink_top, lv_obj_get_y(eyelid_top), EYE_CENTER_Y - lv_obj_get_height(eyelid_top));
    lv_anim_set_duration(&anim_blink_top, DEFAULT_BLINK_CLOSE_DURATION);
    lv_anim_set_reverse_delay(&anim_blink_top, DEFAULT_BLINK_PAUSE_DURATION);
    lv_anim_set_reverse_duration(&anim_blink_top, DEFAULT_BLINK_OPEN_DURATION);
    lv_anim_set_path_cb(&anim_blink_top, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&anim_blink_top, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_start(&anim_blink_top);

    lv_anim_t anim_blink_bottom;
    lv_anim_init(&anim_blink_bottom);
    lv_anim_set_var(&anim_blink_bottom, eyelid_bottom);
    lv_anim_set_values(&anim_blink_bottom, lv_obj_get_y(eyelid_bottom), EYE_CENTER_Y);
    lv_anim_set_duration(&anim_blink_bottom, DEFAULT_BLINK_CLOSE_DURATION);
    lv_anim_set_reverse_delay(&anim_blink_bottom, DEFAULT_BLINK_PAUSE_DURATION);
    lv_anim_set_reverse_duration(&anim_blink_bottom, DEFAULT_BLINK_OPEN_DURATION);
    lv_anim_set_path_cb(&anim_blink_bottom, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&anim_blink_bottom, (lv_anim_exec_xcb_t)lv_obj_set_y);
    lv_anim_start(&anim_blink_bottom);
}

void create_eyelids() {
    const int eyelid_h = EYELID_HEIGHT;
    const int eyelid_w = EYELID_WIDTH;

    eyelid_top = lv_obj_create(lv_screen_active());
    lv_obj_set_size(eyelid_top, eyelid_w, eyelid_h);
    lv_obj_set_pos(eyelid_top, EYE_CENTER_X - (eyelid_w / 2), -eyelid_h); // Start above screen
    lv_obj_set_style_bg_color(eyelid_top, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(eyelid_top, 0, 0);

    eyelid_bottom = lv_obj_create(lv_screen_active());
    lv_obj_set_size(eyelid_bottom, eyelid_w, eyelid_h);
    lv_obj_set_pos(eyelid_bottom, EYE_CENTER_X - (eyelid_w / 2), TFT_VER_RES); // Start below screen
    lv_obj_set_style_bg_color(eyelid_bottom, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(eyelid_bottom, 0, 0);
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
    return millis();
}

void handleScriptedMode(unsigned long currentTime) {
    // restart the sequence if it's the first run
    if (sequenceStartTime == 0) {
      sequenceStartTime = currentTime;
      nextKeyframeIndex = 0;
    }

    unsigned long sequenceTime = currentTime - sequenceStartTime;

    // Check if it's time to trigger the next keyframe
    if (nextKeyframeIndex < NUM_KEYFRAMES && sequenceTime >= sequence[nextKeyframeIndex].startTime) {
        Keyframe currentKeyframe = sequence[nextKeyframeIndex];

        // Send the target positions for the current keyframe
        for (int i = 0; i < NUM_SERVOS; i++) {
            uint8_t channel = SERVO_RANGES[i].channel;
            uint16_t position = currentKeyframe.positions[i];
            
            // Validate position before sending to servo
            if (validateServoPosition(channel, position)) {
                maestro.setTarget(channel, position);
            } else {
                Serial.print("WARNING: Invalid position ");
                Serial.print(position);
                Serial.print(" for channel ");
                Serial.print(channel);
                Serial.println(". Skipping.");
            }
        }

        // Animate the eye to the new position
        uint32_t duration = DEFAULT_EYE_ANIMATION_DURATION; // Default duration for the last frame
        if (nextKeyframeIndex + 1 < NUM_KEYFRAMES) {
            duration = sequence[nextKeyframeIndex + 1].startTime - currentKeyframe.startTime;
        }
        // Validate eye position before animating
        if (validateEyePosition(currentKeyframe.eye_h_pos, currentKeyframe.eye_v_pos) && 
            validateTiming(duration)) {
            animate_eye_to(currentKeyframe.eye_h_pos, currentKeyframe.eye_v_pos, duration);
        } else {
            Serial.print("WARNING: Invalid eye position or timing for keyframe ");
            Serial.println(nextKeyframeIndex);
        }

        nextKeyframeIndex++;
    }

    // Reset the sequence when it's over
    if (nextKeyframeIndex >= NUM_KEYFRAMES) {
        // Optional: add a delay here before looping
        sequenceStartTime = 0; // this will restart the sequence on the next loop
    }
}

// Helper functions for dynamic mode
inline uint16_t max(uint16_t a, uint16_t b) { return (a > b) ? a : b; }
inline uint16_t min(uint16_t a, uint16_t b) { return (a < b) ? a : b; }

void generateDynamicKeyframe(unsigned long currentTime) {
    // Generate procedural servo positions within configured ranges
    for (int i = 0; i < NUM_SERVOS; i++) {
        const ServoRange* range = &SERVO_RANGES[i];
        
        // Calculate movement range based on intensity
        uint16_t centerPos = range->home;
        uint16_t rangeSize = (range->max - range->min) * dynamicConfig.movementIntensity;
        uint16_t minPos = centerPos - (rangeSize / 2);
        uint16_t maxPos = centerPos + (rangeSize / 2);
        
        // Ensure we stay within absolute servo limits
        minPos = max(minPos, range->min);
        maxPos = min(maxPos, range->max);
        
        // Generate random position within calculated range
        uint16_t targetPosition = random(minPos, maxPos + 1);
        
        // Validate and send position
        if (validateServoPosition(range->channel, targetPosition)) {
            maestro.setTarget(range->channel, targetPosition);
        }
    }
    
    // Generate procedural eye movement
    int16_t maxEyeH = EYE_H_RIGHT * dynamicConfig.movementIntensity;
    int16_t maxEyeV = EYE_V_DOWN * dynamicConfig.movementIntensity;
    
    int16_t targetEyeH = random(-maxEyeH, maxEyeH + 1);
    int16_t targetEyeV = random(-maxEyeV, maxEyeV + 1);
    
    // Generate random animation duration
    uint32_t duration = random(dynamicConfig.minHoldDuration, dynamicConfig.maxHoldDuration);
    
    // Validate and animate eyes
    if (validateEyePosition(targetEyeH, targetEyeV) && validateTiming(duration)) {
        animate_eye_to(targetEyeH, targetEyeV, duration);
    }
}

void handleDynamicMode(unsigned long currentTime) {
    // Initialize dynamic mode timing on first run
    if (!dynamicModeInitialized || lastDynamicMovement == 0) {
        lastDynamicMovement = currentTime;
        nextDynamicMovement = currentTime + random(dynamicConfig.minMovementInterval, dynamicConfig.maxMovementInterval);
        return;
    }
    
    // Check if it's time for the next dynamic movement
    if (currentTime >= nextDynamicMovement) {
        generateDynamicKeyframe(currentTime);
        
        // Schedule next movement
        lastDynamicMovement = currentTime;
        nextDynamicMovement = currentTime + random(dynamicConfig.minMovementInterval, dynamicConfig.maxMovementInterval);
    }
}

void setup()
{
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.begin( 9600 );
    Serial.println( LVGL_Arduino );

    maestroSerial.begin(9600, SERIAL_8N1, -1, MAESTRO_TX_PIN);

    // Set speed and acceleration for smooth movements
    // Speed: 0 is unlimited, 1 is 0.25 us / 10 ms, etc.
    // Acceleration: 0 is unlimited, 1 is 0.25 us / 10 ms / 80 ms, etc.
    /* setSpeed takes channel number you want to limit and the
    speed limit in units of (1/4 microseconds)/(10 milliseconds).

    A speed of 0 makes the speed unlimited, so that setting the
    target will immediately affect the position. Note that the
    actual speed at which your servo moves is also limited by the
    design of the servo itself, the supply voltage, and mechanical
    loads. */

    /* setAcceleration takes channel number you want to limit and
    the acceleration limit value from 0 to 255 in units of (1/4
    microseconds)/(10 milliseconds) / (80 milliseconds).

    A value of 0 corresponds to no acceleration limit. An
    acceleration limit causes the speed of a servo to slowly ramp
    up until it reaches the maximum speed, then to ramp down again
    as the position approaches the target, resulting in relatively
    smooth motion from one point to another. */

    // Configure servo motion parameters from config
    Serial.println("Configuring servo motion parameters...");
    for (int i = 0; i < NUM_SERVO_MOTION_CONFIGS; i++) {
        const ServoMotionConfig* config = &SERVO_MOTION_CONFIGS[i];
        maestro.setSpeed(config->channel, config->speed);
        maestro.setAcceleration(config->channel, config->acceleration);
        Serial.print("Channel ");
        Serial.print(config->channel);
        Serial.print(" configured with speed=");
        Serial.print(config->speed);
        Serial.print(", acceleration=");
        Serial.println(config->acceleration);
    }
    
    // Validate servo ranges
    Serial.println("Validating servo configurations...");
    for (int i = 0; i < NUM_SERVOS; i++) {
        const ServoRange* range = &SERVO_RANGES[i];
        if (validateServoPosition(range->channel, range->home)) {
            Serial.print("Channel ");
            Serial.print(range->channel);
            Serial.println(" home position is valid");
        } else {
            Serial.print("WARNING: Channel ");
            Serial.print(range->channel);
            Serial.println(" home position is invalid!");
        }
    }


    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(my_tick);

    /* register print function for debugging */
// #if LV_USE_LOG != 0
//     lv_log_register_print_cb( my_print );
// #endif

    lv_display_t * disp;
#if LV_USE_TFT_ESPI
    /*TFT_eSPI can be enabled lv_conf.h to initialize the display in a simple way*/
    disp = lv_tft_espi_create(TFT_HOR_RES, TFT_VER_RES, draw_buf, sizeof(draw_buf));
    lv_display_set_rotation(disp, TFT_ROTATION);

#else
    /*Else create a display yourself*/
    disp = lv_display_create(TFT_HOR_RES, TFT_VER_RES);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    /*Initialize the (dummy) input device driver*/
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER); /*Touchpad should have POINTER type*/
    lv_indev_set_read_cb(indev, my_touchpad_read);

    // set background color (for debug, use 0x00ff00)
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);

    // draw our eye
    draw_outer_eyeball(EYE_CENTER_X, EYE_CENTER_Y, 0, 0);

    // create eyelids
    create_eyelids();

    // Init blink timer
    randomSeed(analogRead(0)); // Seed random number generator
    last_blink_time = millis();
    next_blink_interval = random(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);

    // Test configuration system
    Serial.println("Testing configuration system...");
    
    // Test servo range lookup
    const ServoRange* panRange = getServoRange(SKULL_PAN_CHANNEL);
    if (panRange != nullptr) {
        Serial.print("Pan servo range: ");
        Serial.print(panRange->min);
        Serial.print(" to ");
        Serial.println(panRange->max);
    }
    
    // Test motion config lookup
    const ServoMotionConfig* panMotion = getServoMotionConfig(SKULL_PAN_CHANNEL);
    if (panMotion != nullptr) {
        Serial.print("Pan servo motion - Speed: ");
        Serial.print(panMotion->speed);
        Serial.print(", Acceleration: ");
        Serial.println(panMotion->acceleration);
    }
    
    // Initialize operation mode
    Serial.println("Initializing operation mode...");
    switch (currentMode) {
        case OperationMode::SCRIPTED:
            Serial.println("Mode: SCRIPTED - Using predefined keyframe sequences");
            // Scripted mode uses the existing sequence array - no additional setup needed
            break;
        case OperationMode::DYNAMIC:
            Serial.println("Mode: DYNAMIC - Using procedural movement generation");
            Serial.print("Movement intensity: ");
            Serial.println(dynamicConfig.movementIntensity);
            Serial.print("Movement interval: ");
            Serial.print(dynamicConfig.minMovementInterval);
            Serial.print(" - ");
            Serial.print(dynamicConfig.maxMovementInterval);
            Serial.println(" ms");
            dynamicModeInitialized = true;
            break;
    }
    
    Serial.println( "Setup done" );
}

void loop()
{
    unsigned long currentTime = millis();

    // Automatic blinker
    if (currentTime - last_blink_time > next_blink_interval) {
        trigger_blink();
        last_blink_time = currentTime;
        next_blink_interval = random(BLINK_INTERVAL_MIN_MS, BLINK_INTERVAL_MAX_MS);
    }

    // Handle animation based on current operation mode
    switch (currentMode) {
        case OperationMode::SCRIPTED:
            handleScriptedMode(currentTime);
            break;
        case OperationMode::DYNAMIC:
            handleDynamicMode(currentTime);
            break;
    }

    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}