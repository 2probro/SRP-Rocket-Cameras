#include <Arduino.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_random.h> // Include for random number generation

// Pin definitions for AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// Launch detection and status LED pins
#define BREAK_PIN         12    // Break-wire input (pullup)
#define LED_PIN           4    // Onboard FLASH LED

// Recording settings
#define RECORD_DURATION   120    // 2 minutes in seconds ACTUAL LAUNCH
// #define RECORD_DURATION   10    // 10 seconds in seconds FOR TESTING

// Optional audio recording (not implemented)
#define ENABLE_AUDIO      0

void setup() {
  // Disable brown-out detector (helps prevent resets when SD writes)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  pinMode(BREAK_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  char folder_name[12]; // Stores the unique session folder name, e.g., "/A1B2C3D4"

  Serial.println("Starting setup...");

  // Flash LED once to signal power on
  digitalWrite(LED_PIN, HIGH);
  delay(200);
  digitalWrite(LED_PIN, LOW);
  delay(200);

  // --- Initialize camera ---
  Serial.println("Initializing camera...");
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  // Select optimal settings based on PSRAM availability
  // PSRAM allows for higher resolution and quality
  if (psramFound()) {
    config.frame_size   = FRAMESIZE_SXGA;    // max resolution if PSRAM present
    config.jpeg_quality = 20;                // Slightly lower quality for smaller files/faster writes (higher number = lower quality)
    config.fb_count     = 2;                 // double-buffering
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;    // fallback resolution
    config.jpeg_quality = 15;                // standard quality
    config.fb_count     = 1;                 // single buffer
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (1) delay(1000); // Halt
  }

  // Sensor tuning: adjust image settings after camera initialization
  sensor_t * s = esp_camera_sensor_get(); // get pointer to sensor settings
  s->set_brightness(s, 0);       // brightness: -2 (dark) to +2 (bright), default 0
  s->set_contrast(s, 0);         // contrast: -2 (low) to +2 (high), default 0
  s->set_saturation(s, 0);       // saturation: -2 (desaturated) to +2 (vivid), default 0
  s->set_special_effect(s, 0);   // special effect: 0 none, 1 negative, 2 grayscale, 3 red, 4 green, 5 blue, 6 sepia
  s->set_whitebal(s, 1);         // auto white balance: 0 disable, 1 enable
  s->set_awb_gain(s, 1);         // AWB gain: 0 disable, 1 enable
  s->set_wb_mode(s, 0);          // white balance mode: 0 auto, 1 sunny, 2 cloudy, 3 office, 4 home
  s->set_exposure_ctrl(s, 1);    // automatic exposure control: 0 disable, 1 enable
  s->set_aec2(s, 0);             // second auto-exposure algorithm: 0 disable, 1 enable
  s->set_ae_level(s, 0);         // AE exposure level compensation: -2 to +2, default 0
  s->set_aec_value(s, 300);      // target AE value: 0 to 1200 (higher → brighter)
  s->set_gain_ctrl(s, 1);        // automatic gain control: 0 disable, 1 enable
  s->set_agc_gain(s, 0);         // manual analog gain: 0 to 30, default 0
  s->set_gainceiling(s, (gainceiling_t)0); // gain ceiling: 0 (2x) to 6 (128x)
  s->set_bpc(s, 0);              // black pixel correction: 0 disable, 1 enable
  s->set_wpc(s, 1);              // white pixel correction: 0 disable, 1 enable
  s->set_raw_gma(s, 1);          // gamma correction: 0 disable, 1 enable
  s->set_lenc(s, 1);             // lens distortion correction: 0 disable, 1 enable
  s->set_hmirror(s, 0);          // horizontal mirror: 0 disable, 1 enable
  s->set_vflip(s, 0);            // vertical flip: 0 disable, 1 enable
  s->set_dcw(s, 1);              // downsize control: 0 disable, 1 enable
  s->set_colorbar(s, 0);         // color bar test pattern: 0 disable, 1 enable

  // --- Initialize SD card ---
  Serial.println("Initializing SD card...");
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card mount failed");
    while (1) delay(1000); // Halt
  }

  // Flash 2 times to signal SD Card and Camera initialization worked
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  // Create a unique session folder
  Serial.println("Creating session folder...");
  uint32_t random_num = esp_random();
  snprintf(folder_name, sizeof(folder_name), "/%08X", random_num);
  String temp_session_dir_for_mkdir(folder_name); // Use a temporary String for mkdir path
  if (!SD_MMC.mkdir(temp_session_dir_for_mkdir.c_str())) {
    Serial.println("ERROR: Failed to create session directory. Halting.");
    // Flash LED rapidly and continuously to indicate critical error
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(50);
      digitalWrite(LED_PIN, LOW); delay(50);
    }
  } else {
    Serial.printf("Session folder %s created successfully.\n", temp_session_dir_for_mkdir.c_str());
  }

  // --- Breakwire logic ---
  // Wait for breakwire to be connected if not already
  if (digitalRead(BREAK_PIN) == HIGH) {
    Serial.println("Waiting for breakwire to be connected...");
    while (digitalRead(BREAK_PIN) == HIGH) {
      delay(10);
    }
    Serial.println("Breakwire connected.");
    // Flash 3 times to signal breakwire connected
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  } else {
    Serial.println("Breakwire already connected at startup.");
    // Flash 3 times to signal breakwire connected
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
    }
  }

  Serial.println("Breakwire connected, waiting for launch...");

  // While breakwire is connected, wait, until it is disconnected
  while (digitalRead(BREAK_PIN) == LOW) {
    delay(5);
  }

  // Recording is starting!
  Serial.println("Recording is starting!");

  // Open MJPEG file in the pre-created session folder
  String session_dir(folder_name); // Uses folder_name populated earlier
  String mjpeg_path = session_dir + "/video.MJPEG";
  File mjpeg_file = SD_MMC.open(mjpeg_path.c_str(), FILE_WRITE);
  if (!mjpeg_file) {
    Serial.println("ERROR: Failed to open MJPEG file for writing. Halting.");
    // Flash LED rapidly and continuously to indicate critical error
    while (1) {
      digitalWrite(LED_PIN, HIGH); delay(50);
      digitalWrite(LED_PIN, LOW); delay(50);
    }
  }

  // Main recording loop (append all frames to MJPEG container)
  unsigned long start_ms = millis();
  uint32_t frame_count = 0;
  Serial.printf("Recording MJPEG to %s...\n", mjpeg_path.c_str());
  while ((millis() - start_ms) < RECORD_DURATION * 1000UL) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Frame capture failed");
      continue;
    }

    // Append JPEG buffer to MJPEG container
    if (mjpeg_file) {
      mjpeg_file.write(fb->buf, fb->len);
    }
    Serial.printf("Appended frame %06u (%u bytes)\n", frame_count++, fb->len);
    esp_camera_fb_return(fb);
  }

  // Finish recording
  if (mjpeg_file) {
    mjpeg_file.close();
    Serial.println("MJPEG recording complete");
  }

  // Flash LED 4 times quickly to signal recording done
  for (int i = 0; i < 4; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}

void loop() {
  // Not used
} 