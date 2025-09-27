/*Using LVGL with Arduino requires some extra steps:
 *Be sure to read the docs here: https://docs.lvgl.io/master/integration/framework/arduino.html  */

#include <lvgl.h>

#if LV_USE_TFT_ESPI
#include <TFT_eSPI.h>
#endif

#include <HardwareSerial.h>
#include <PololuMaestro.h>
 
HardwareSerial maestroSerial(2); 
static const int MAESTRO_TX_PIN = 21;
static const int SKULL_PAN_CHANNEL = 0;
static const int SKULL_NOD_CHANNEL = 1;
static const int SKULL_JAW_CHANNEL = 2; 
MiniMaestro maestro(maestroSerial);

/*Set to your screen resolution and rotation*/
#define TFT_HOR_RES   240
#define TFT_VER_RES   240
#define TFT_ROTATION  LV_DISPLAY_ROTATION_0

/*LVGL draw into this buffer, 1/10 screen size usually works well. The size is in bytes*/
#define DRAW_BUF_SIZE (TFT_HOR_RES * TFT_VER_RES / 10 * (LV_COLOR_DEPTH / 8))
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

uint32_t pack_x_y(int16_t x, int16_t y)
{
    return ( (int32_t)y << 16) | x;
}

static void anim_pupil_x_y(void * obj, int32_t v)
{
    // receives eye object, gets pupil (child), and animates position
    int16_t x = v & 0xFFFF;
    int16_t y = (v >> 16) & 0xFFFF;

    lv_obj_t * pupil = lv_obj_get_child((lv_obj_t *) obj, NULL);

    lv_obj_set_pos(pupil, x, y);
}

static void anim_sclera_x_y(void * obj, int32_t v)
{
    // receives the eye, gets the background style, and animates the focal
    int16_t x = v & 0xFFFF;
    int16_t y = (v >> 16) & 0xFFFF;
    const int inner_radius = 30;
    SCLERA_GRADIENT.params.radial.focal.x = x;
    SCLERA_GRADIENT.params.radial.focal.y = y;
    SCLERA_GRADIENT.params.radial.focal_extent.x = x + inner_radius;
    SCLERA_GRADIENT.params.radial.focal_extent.y = y + inner_radius;

    lv_obj_invalidate((lv_obj_t *)obj);

}

void draw_outer_eyeball(int center_x, int center_y, int offset_x, int offset_y)
{
    // Gradient Docs: https://docs.lvgl.io/9.3/details/main-modules/draw/draw_descriptors.html#radial-gradients and
    // https://github.com/lvgl/lvgl/blob/master/examples/SCLERA_GRADIENT/lv_example_grad_3.c
    const int outer_radius = 80;
    const int inner_radius = 30;
    const int pupil_radius = 20;

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
    lv_obj_t * eye = lv_obj_create(lv_screen_active());
    lv_obj_add_style(eye, &style, 0);
    lv_obj_set_size(eye, lv_pct(100), lv_pct(100));
    lv_obj_center(eye);

    // pupil
    lv_obj_t * pupil = lv_obj_create(eye);
    lv_obj_set_size(pupil, pupil_radius, pupil_radius);
    // set_pos is upper-left corner
    lv_obj_set_pos(pupil, center_x - pupil_radius + offset_x, center_y - pupil_radius + offset_y);
    lv_obj_set_style_bg_color(pupil, lv_color_hex(0xffcc00), 0);
    lv_obj_set_style_border_width(pupil, 0, 0);
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);

    static lv_anim_t anim;
    static lv_anim_t * running_anum;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, eye);
    offset_x = 40;
    uint32_t start = pack_x_y(center_x - pupil_radius - offset_x, center_y - pupil_radius);
    uint32_t end = pack_x_y(center_x - pupil_radius + offset_x, center_y - pupil_radius + offset_y);
    lv_anim_set_values(&anim, start, end);
    lv_anim_set_duration(&anim, 500);
    lv_anim_set_reverse_delay(&anim, 1000);
    lv_anim_set_reverse_duration(&anim, 500);
    lv_anim_set_repeat_delay(&anim, 500);
    lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&anim, anim_pupil_x_y);
    lv_anim_start(&anim);

    // and the sclera
    start = pack_x_y(center_x - offset_x, center_y);
    end = pack_x_y(center_x + offset_x, center_y);
    lv_anim_set_values(&anim, start, end);
    lv_anim_set_exec_cb(&anim, anim_sclera_x_y);
    lv_anim_start(&anim);


    // test moving it afterwards
    // lv_obj_set_pos(pupil, center_x - pupil_radius + offset_x, center_y - pupil_radius + offset_y);
    // lv_grad_radial_set_focal(&SCLERA_GRADIENT, center_x + offset_x, center_y + offset_y, inner_radius);
}

/*use Arduinos millis() as tick source*/
static uint32_t my_tick(void)
{
    return millis();
}

void setup()
{
    String LVGL_Arduino = "Hello Arduino! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    Serial.begin( 115200 );
    Serial.println( LVGL_Arduino );

    maestroSerial.begin(9600, SERIAL_8N1, -1, MAESTRO_TX_PIN);

    lv_init();

    /*Set a tick source so that LVGL will know how much time elapsed. */
    lv_tick_set_cb(my_tick);

    /* register print function for debugging */
#if LV_USE_LOG != 0
    lv_log_register_print_cb( my_print );
#endif

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
    draw_outer_eyeball(120, 120, 0, 0);

    Serial.println( "Setup done" );
}

void loop()
{
    maestro.setTarget(SKULL_PAN_CHANNEL, 5600);
    lv_timer_handler(); /* let the GUI do its work */
    delay(5); /* let this time pass */
}