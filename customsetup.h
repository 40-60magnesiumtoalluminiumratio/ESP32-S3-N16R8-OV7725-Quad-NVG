#define USER_SETUP_INFO "User_Setup_ST7789_ESP32S3"

#define ST7789_DRIVER      

#define TFT_RGB_ORDER TFT_BGR // might need to change to RGB if the colors are wrong

// ST7789/GC9A01 resolution
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_INVERSION_ON

// ====== pinout ======
#define TFT_MOSI 19   // sda
#define TFT_SCLK 20   // scl (often labeled as clk)
#define TFT_CS   21   // cs
#define TFT_DC   47   // dc
#define TFT_RST  48   // rst
#define TFT_BL   45   // bl

// ====== fonts ======
#define LOAD_GLCD   
#define LOAD_FONT2  
#define LOAD_FONT4  
#define LOAD_FONT6  
#define LOAD_FONT7  
#define LOAD_FONT8  
#define LOAD_GFXFF  
#define SMOOTH_FONT 

// ====== SPI freq ======
#define SPI_FREQUENCY  80000000 // fastest available on st7789
#define SPI_READ_FREQUENCY  27000000 // irrelevant in our case
#define SPI_TOUCH_FREQUENCY  2500000 // same thing

#define USE_HSPI_PORT // tested with hspi, fspi may be used but pin conflicts may occur
