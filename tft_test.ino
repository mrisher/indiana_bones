#include <lvgl.h>
#include "tft_setup.h"
#include "TFT_eSPI.h"
#include "demon_eye_argb.h"

TFT_eSPI tft = TFT_eSPI();

const int cw = tft.width()/2;
const int ch = tft.height()/2;
const int s = min(cw/4,ch/4);

// Define eye colors
#define SCLERA_COLOR 0xef7c  // Off-white
#define IRIS_COLOR   TFT_BLUE
#define PUPIL_COLOR  TFT_BLACK
#define BACKGROUND_COLOR TFT_BLACK

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

void setup(void) {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BACKGROUND_COLOR);
  drawEye();
}

void loop() {
  // Animate the blink
  for (int i = 0; i <= s; i += 2) {
    // Top lid
    tft.fillCircle(ch, cw - i, s * 2.1, BACKGROUND_COLOR);
    // Bottom lid
    tft.fillCircle(ch, cw + i, s * 2.1, BACKGROUND_COLOR);
    delay(20);
  }

  // Draw the eye again to simulate opening
  drawEye();
  delay(1000);

  // Animate the blink again
  for (int i = s; i >= 0; i -= 2) {
    // Top lid
    tft.fillCircle(ch, cw - i, s * 2.1, BACKGROUND_COLOR);
    // Bottom lid
    tft.fillCircle(ch, cw + i, s * 2.1, BACKGROUND_COLOR);
    delay(20);
  }

  // Draw the eye again to ensure it's fully open
  drawEye();
  delay(1000);
}