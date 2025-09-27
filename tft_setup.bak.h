// 1.3" TFT Display (GMT130 V.10), 
// no CS pin
// 240x240, ST7789
// tft_setup.h
#define ST7789_DRIVER

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_RGB_ORDER TFT_BGR

#define TFT_CS    5   
#define TFT_RST   16  
#define TFT_DC    17  
#define TFT_MOSI  23  // SDA // HW MOSI
#define TFT_SCLK  18  // SCL // HW SCL
#define TFT_MISO  19  // not used
#define TFT_BL    22  // LED back-light
#define TFT_BACKLIGHT_ON HIGH


#define LOAD_GLCD   
#define LOAD_FONT2  
#define LOAD_FONT4  
#define LOAD_FONT6  
#define LOAD_FONT7  
#define LOAD_FONT8  
#define LOAD_GFXFF
#define SMOOTH_FONT 

#define SPI_FREQUENCY  27000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  2500000