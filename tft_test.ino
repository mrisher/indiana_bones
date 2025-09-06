#include <lvgl.h>
#include "tft_setup.h"
#include "TFT_eSPI.h"
#include "demon_eye_argb.h"

TFT_eSPI tft = TFT_eSPI();

const int cw = tft.width()/2;
const int ch = tft.height()/2;
const int s = min(cw/4,ch/4);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
#define BACKGROUND_COLOR TFT_BLACK

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

// If logging is enabled, it will inform the user about what is happening in the library
void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

void drawEye() {

// https://docs.lvgl.io/8/get-started/platforms/arduino.html
  LV_IMG_DECLARE(demon_eye);
  lv_obj_t * img1 = lv_image_create(lv_screen_active());
  lv_image_set_src(img1, &demon_eye);
  lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

  const int EYE_DIMENSION = 200;

  // // First, draw the pre-rendered sclera from the image data
  // tft.pushImage(ch - EYE_DIMENSION / 2, cw - EYE_DIMENSION / 2, EYE_DIMENSION, EYE_DIMENSION, imageData);

  // tft.fillCircle(ch, cw, s * 2, SCLERA_COLOR);
  // tft.fillCircle(ch, cw, s, IRIS_COLOR);
  // tft.fillCircle(ch, cw, s * 0.5, PUPIL_COLOR);
}

// uint_64 my_tick_get_cb(void) {
//   return esp_timer_get_time() / 1000;
// } 

void setup(void) {

  // tft.init();
  // tft.setRotation(1);
  // tft.fillScreen(BACKGROUND_COLOR);
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLUE);
  
  // // Start LVGL
  lv_init();
  // // // Register print function for debugging
  // // lv_log_register_print_cb(log_print);

  // Create a display object
  lv_display_t * disp;
  // Initialize the TFT display using the TFT_eSPI library
  // disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  // lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
  
  
  // // drawEye();
  //     /*Change the active screen's background color*/
  //   lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

  //   /*Create a white label, set its text and align it to the center*/
  //   lv_obj_t * label = lv_label_create(lv_screen_active());
  //   lv_label_set_text(label, "Hello world");
  //   lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
  //   lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}


void loop() {
  // // Animate the blink
  // for (int i = 0; i <= s; i += 2) {
  //   // Top lid
  //   tft.fillCircle(ch, cw - i, s * 2.1, BACKGROUND_COLOR);
  //   // Bottom lid
  //   tft.fillCircle(ch, cw + i, s * 2.1, BACKGROUND_COLOR);
  //   delay(20);
  // }

  // // Draw the eye again to simulate opening
  // drawEye();
  // delay(1000);

  // // Animate the blink again
  // for (int i = s; i >= 0; i -= 2) {
  //   // Top lid
  //   tft.fillCircle(ch, cw - i, s * 2.1, BACKGROUND_COLOR);
  //   // Bottom lid
  //   tft.fillCircle(ch, cw + i, s * 2.1, BACKGROUND_COLOR);
  //   delay(20);
  // }

  // // Draw the eye again to ensure it's fully open
  // drawEye();
  // delay(1000);
}