#include <TFT_eSPI.h>
#include <EEPROM.h>          
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

// Pin Definitions for Peripherals
#define POT_PIN            1
#define BUTTON_PIN         2   

// UART Broadcast Pins
#define MASTER_TX_PIN     21  // Connects to RX pins of all three slaves
#define MASTER_RX_PIN     -1  // Unused

// ==================== CONFIGURABLE BOUNDARIES ====================
#define POT_MIN_ADC       100   
#define POT_MAX_ADC       4095  

#define EXPOSURE_MIN      50    
#define EXPOSURE_MAX      1500  
#define GAIN_MIN          0     
#define GAIN_MAX          30    

const float SMOOTH_ALPHA = 0.25; 
float smoothedPotVal = 0.0;

// Set to 'false' if your camera module experiences stability issues with higher clock dividers
#define ENABLE_CAMERA_OVERCLOCK  true  
// =================================================================

// ==================== EEPROM CONFIGURATION =======================
#define EEPROM_SIZE       16    
#define EXPOSURE_ADDR      0    
#define GAIN_ADDR          4    
#define SIG_ADDR           8    
#define EEPROM_SIGNATURE 0x55   

bool eepromPending = false;
unsigned long lastChangeTime = 0;
const unsigned long EEPROM_WRITE_DELAY = 200; 
// =================================================================

enum ControlMode {
  MODE_EXPOSURE,
  MODE_GAIN
};
ControlMode currentMode = MODE_EXPOSURE;

int savedExposure = 300; 
int savedGain = 10;      

// Parameter-jumping prevention states
bool potLocked = false;
int potValueAtSwitch = 0;

unsigned long lastFrameTime = 0;
float fps = 0;

// Sends the current exposure and gain settings to the broadcast line
void sendParamsToSlaves() {
  Serial1.printf("#E%dG%d\n", savedExposure, savedGain);
}

void checkButton(int currentPotVal) {
  int reading = digitalRead(BUTTON_PIN);
  static int lastButtonState = HIGH;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50; 

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    static int buttonState = HIGH;
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) { 
        if (currentMode == MODE_EXPOSURE) {
          currentMode = MODE_GAIN;
        } else {
          currentMode = MODE_EXPOSURE;
        }
        // Lock the pot upon switching modes to prevent parameter jumping
        potLocked = true;
        potValueAtSwitch = currentPotVal;
        Serial.printf("Mode switched. Potentiometer locked at: %d\n", potValueAtSwitch);
      }
    }
  }
  lastButtonState = reading;
}

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

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("\n--- Starting Diagnostics ---");
  
  // Initialize Serial1 for broadcasting parameters
  Serial1.begin(115200, SERIAL_8N1, MASTER_RX_PIN, MASTER_TX_PIN);

  analogReadResolution(12);
  pinMode(POT_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize Display
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLUE); 
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.setTextSize(2);
  tft.drawString("Init System...", 10, 10);
  delay(1000);
  
  // Initialize EEPROM
  tft.drawString("Mounting Memory...", 10, 40);
  delay(1000);
  if (!EEPROM.begin(EEPROM_SIZE)) {
    tft.fillScreen(TFT_RED);
    tft.drawString("EEPROM Failure!", 10, 10);
    Serial.println("Error: EEPROM partition failed to mount.");
    while(true) delay(1000);
  }

  // Load configuration parameters
  byte signature = EEPROM.read(SIG_ADDR);
  if (signature == EEPROM_SIGNATURE) {
    EEPROM.get(EXPOSURE_ADDR, savedExposure);
    EEPROM.get(GAIN_ADDR, savedGain);
    Serial.printf("Loaded Configurations - Exposure: %d, Gain: %d\n", savedExposure, savedGain);
  } else {
    EEPROM.put(EXPOSURE_ADDR, savedExposure);
    EEPROM.put(GAIN_ADDR, savedGain);
    EEPROM.write(SIG_ADDR, EEPROM_SIGNATURE);
    EEPROM.commit();
    Serial.println("First-run signature created. Default values written.");
  }

  // Initialize Camera
  tft.drawString("Starting Cam...", 10, 70);
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

  // Sync up slave parameters with loaded startup parameters
  sendParamsToSlaves();

  // --- STARTUP PARAMETER JUMPING PROTECTION ---
  int initialPotVal = analogRead(POT_PIN);
  smoothedPotVal = initialPotVal;

  // Map the dial's physical start position to the exposure scale (default startup mode)
  int potClamped = constrain(initialPotVal, POT_MIN_ADC, POT_MAX_ADC);
  int initialMappedExposure = map(potClamped, POT_MIN_ADC, POT_MAX_ADC, EXPOSURE_MIN, EXPOSURE_MAX);

  // If the physical dial does not align with the loaded EEPROM exposure, lock the control
  if (abs(initialMappedExposure - savedExposure) > 5) {
    potLocked = true;
    potValueAtSwitch = initialPotVal;
    Serial.printf("Startup Mismatch (Dial maps to: %d vs EEPROM: %d). Potentiometer locked.\n", initialMappedExposure, savedExposure);
  } else {
    potLocked = false;
    Serial.println("Startup alignment check passed. Potentiometer active.");
  }

  tft.fillScreen(TFT_BLACK);
}

void loop() {
  int rawPotValue = analogRead(POT_PIN);
  smoothedPotVal = (SMOOTH_ALPHA * rawPotValue) + ((1.0 - SMOOTH_ALPHA) * smoothedPotVal);
  int currentPotVal = (int)smoothedPotVal;

  checkButton(currentPotVal);

  // Unlock physical controls if user rotates the dial past deadband threshold
  if (potLocked) {
    if (abs(currentPotVal - potValueAtSwitch) > 80) { 
      potLocked = false;
    }
  }

  int potClamped = constrain(currentPotVal, POT_MIN_ADC, POT_MAX_ADC);

  if (!potLocked) {
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
      if (currentMode == MODE_EXPOSURE) {
        int targetExposure = map(potClamped, POT_MIN_ADC, POT_MAX_ADC, EXPOSURE_MIN, EXPOSURE_MAX);
        if (abs(targetExposure - savedExposure) > 5) {
          s->set_aec_value(s, targetExposure);
          savedExposure = targetExposure;
          eepromPending = true;       
          lastChangeTime = millis();  
          sendParamsToSlaves();  // Broadcast changes
        }
      } 
      else if (currentMode == MODE_GAIN) {
        int targetGain = map(potClamped, POT_MIN_ADC, POT_MAX_ADC, GAIN_MIN, GAIN_MAX);
        if (abs(targetGain - savedGain) > 0) {
          s->set_agc_gain(s, targetGain);
          savedGain = targetGain;
          eepromPending = true;       
          lastChangeTime = millis();  
          sendParamsToSlaves();  // Broadcast changes
        }
      }
    }
  }

  if (eepromPending && (millis() - lastChangeTime > EEPROM_WRITE_DELAY)) {
    EEPROM.put(EXPOSURE_ADDR, savedExposure);
    EEPROM.put(GAIN_ADDR, savedGain);
    EEPROM.commit();                 
    eepromPending = false;
    Serial.println("Auto-saved configurations to EEPROM");
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(10);
    return;
  }

  if (fb->format == PIXFORMAT_RGB565 && fb->width == 240 && fb->height == 240) {
    unsigned long start = micros();
    tft.pushImage(0, 0, 240, 240, (uint16_t *)fb->buf);
    unsigned long latency = micros() - start;

    tft.drawCircle(120, 120, 3, TFT_WHITE);

    unsigned long now = millis();
    if (lastFrameTime != 0) {
      fps = 1000.0 / (now - lastFrameTime);
    }
    lastFrameTime = now;

    tft.setTextSize(1);
    
    // Line 1: Exposure
    tft.setCursor(120, 200);
    if (currentMode == MODE_EXPOSURE) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK); 
      tft.printf("> Exposure: %-4d %s", savedExposure, potLocked ? "[L]" : (eepromPending ? "[*]" : "   "));
    } else {
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK); 
      tft.printf("  Exposure: %-4d    ", savedExposure);
    }
    
    // Line 2: Gain
    tft.setCursor(120, 210);
    if (currentMode == MODE_GAIN) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK); 
      tft.printf("> Gain: %-2d      %s", savedGain, potLocked ? "[L]" : (eepromPending ? "[*]" : "   "));
    } else {
      tft.setTextColor(TFT_DARKGREY, TFT_BLACK); 
      tft.printf("  Gain: %-2d         ", savedGain);
    }

    // Line 3: Stats
    tft.setCursor(120, 220);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.printf("FPS: %.1f | %lu ms ", fps, latency / 1000);
    
  } else {
    Serial.printf("Unexpected frame: %dx%d format %d\n", fb->width, fb->height, fb->format);
  }
  
  esp_camera_fb_return(fb);
}
