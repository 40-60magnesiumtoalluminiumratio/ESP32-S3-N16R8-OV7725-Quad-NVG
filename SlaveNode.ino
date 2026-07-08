#include <TFT_eSPI.h>
#include "esp_camera.h"

TFT_eSPI tft = TFT_eSPI();

// ESP32-S3-WROOM-1-N16R8 Standard Camera Pin Definitions (CAMERA_MODEL_ESP32S3_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM      4
#define SIOC_GPIO_NUM      5
#define VSYNC_GPIO_NUM     6
#define HREF_GPIO_NUM      7
#define PCLK_GPIO_NUM     13

// S3 standard 8-bit data bus pins
#define Y2_GPIO_NUM       11  // Data 0
#define Y3_GPIO_NUM        9  // Data 1
#define Y4_GPIO_NUM        8  // Data 2
#define Y5_GPIO_NUM       10  // Data 3
#define Y6_GPIO_NUM       12  // Data 4
#define Y7_GPIO_NUM       18  // Data 5
#define Y8_GPIO_NUM       17  // Data 6
#define Y9_GPIO_NUM       16  // Data 7

// UART RX Pin 
#define SLAVE_RX_PIN      21  // Connects to Master's TX pin
#define SLAVE_TX_PIN      -1  // Unused

// ==================== CONFIGURABLE BOUNDARIES ====================
#define EXPOSURE_MIN      50    
#define EXPOSURE_MAX      1500  
#define GAIN_MIN          0     
#define GAIN_MAX          30    

// Set to 'false' if your camera module experiences stability issues with higher clock dividers
#define ENABLE_CAMERA_OVERCLOCK  true  
// =================================================================

// Internal state parameters, updated by UART messages
int savedExposure = 300; 
int savedGain = 10;      

esp_err_t setupCamera() {
  camera_config_t config;

  config.ledc_channel     = LEDC_CHANNEL_0;
  config.ledc_timer       = LEDC_TIMER_0;

  config.pin_d0           = Y2_GPIO_NUM;
  config.pin_d1           = Y3_GPIO_NUM;
  config.pin_d2           = Y4_GPIO_NUM;
  config.pin_d3           = Y5_GPIO_NUM;
  config.pin_d4           = Y6_GPIO_NUM;
  config.pin_d5           = Y7_GPIO_NUM;
  config.pin_d6           = Y8_GPIO_NUM;
  config.pin_d7           = Y9_GPIO_NUM;

  config.pin_xclk         = XCLK_GPIO_NUM;
  config.pin_pclk         = PCLK_GPIO_NUM;
  config.pin_vsync        = VSYNC_GPIO_NUM;
  config.pin_href         = HREF_GPIO_NUM;

  config.pin_sscb_sda     = SIOD_GPIO_NUM;
  config.pin_sscb_scl     = SIOC_GPIO_NUM;

  config.pin_pwdn         = PWDN_GPIO_NUM;
  config.pin_reset        = RESET_GPIO_NUM;

  config.xclk_freq_hz     = 160000000; 
  config.pixel_format     = PIXFORMAT_RGB565;
  config.frame_size       = FRAMESIZE_240X240;
  config.fb_count         = 2;
  config.fb_location      = CAMERA_FB_IN_DRAM;
  config.grab_mode        = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("Warning: PSRAM not found or not enabled (Using SRAM for camera)");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    return err; 
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_exposure_ctrl(s, 0); 
    s->set_aec2(s, 0);          
    s->set_gain_ctrl(s, 0);     
    
    s->set_aec_value(s, savedExposure);
    s->set_agc_gain(s, savedGain);     

    if (ENABLE_CAMERA_OVERCLOCK) {
      s->set_reg(s, 0xFF, 0xFF, 0x01); 
      s->set_reg(s, 0x11, 0xFF, 0x81); 
    }
  }
  return ESP_OK;
}

void parseParams(char* data) {
  // Expected incoming structure format: "#E300G10"
  if (data[0] != '#') return;
  
  char* ePtr = strchr(data, 'E');
  char* gPtr = strchr(data, 'G');
  
  if (ePtr && gPtr) {
    int expVal = atoi(ePtr + 1);
    int gainVal = atoi(gPtr + 1);
    
    // Bounds validation checks
    if (expVal >= EXPOSURE_MIN && expVal <= EXPOSURE_MAX &&
        gainVal >= GAIN_MIN && gainVal <= GAIN_MAX) {
      
      sensor_t *s = esp_camera_sensor_get();
      if (s != NULL) {
        if (expVal != savedExposure) {
          s->set_aec_value(s, expVal);
          savedExposure = expVal;
          Serial.printf("Slave updated Exposure to: %d\n", savedExposure);
        }
        if (gainVal != savedGain) {
          s->set_agc_gain(s, gainVal);
          savedGain = gainVal;
          Serial.printf("Slave updated Gain to: %d\n", savedGain);
        }
      }
    }
  }
}

// Non-blocking serial listener
void checkSerialParams() {
  static char buffer[32];
  static int index = 0;
  
  while (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c == '\n') {
      buffer[index] = '\0';
      parseParams(buffer);
      index = 0;
    } else if (index < (int)sizeof(buffer) - 1) {
      buffer[index++] = c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n--- Starting Diagnostics (SLAVE) ---");
  
  // Initialize Serial1 interface to read broadcasts from the Master
  Serial1.begin(115200, SERIAL_8N1, SLAVE_RX_PIN, SLAVE_TX_PIN);

  // Initialize Display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLUE); 
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("Init Slave...", 10, 10);
  delay(1000);

  // Initialize Camera
  tft.drawString("Starting Cam...", 10, 40);
  esp_err_t err = setupCamera();
  if (err != ESP_OK) {
    tft.fillScreen(TFT_RED);
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("Cam Init Fail", 10, 10);
    tft.setTextSize(1);
    tft.setCursor(10, 50);
    tft.printf("Error Code: 0x%x", err);
    tft.setCursor(10, 70);
    tft.printf("Check physical camera connection.");
    Serial.printf("Camera initialization failed with error: 0x%x\n", err);
    while (true) delay(1000);
  }

  tft.fillScreen(TFT_BLACK);
}

void loop() {
  // Check for incoming parameter updates from the Master
  checkSerialParams();

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(10);
    return;
  }

  if (fb->format == PIXFORMAT_RGB565 && fb->width == 240 && fb->height == 240) {
    // Render raw image stream with zero text or interface overlays
    tft.pushImage(0, 0, 240, 240, (uint16_t *)fb->buf);
  } else {
    Serial.printf("Unexpected frame: %dx%d format %d\n", fb->width, fb->height, fb->format);
  }
  
  esp_camera_fb_return(fb);
}
