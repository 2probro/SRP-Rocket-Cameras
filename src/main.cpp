#include <Arduino.h>
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include <esp_random.h> // Include for random number generation

// LAUNCH: make sure to set DANGEROUS_DEBUGGING_MODE to 0 and RECORD_DURATION to 120 for actual launch

// MUST BE SET TO 0 FOR ACTUAL LAUNCH
#define DANGEROUS_DEBUGGING_MODE 1 // set to 1 to skip deep sleep during testing

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
// #define RECORD_DURATION   120    // 2 minutes in seconds ACTUAL LAUNCH
#define RECORD_DURATION   10    // 10 seconds in seconds FOR TESTING

#define FRAME_INTERVAL    (1000 / 15)  // ms per frame for ~15 fps

// Optional audio recording (not implemented)
#define ENABLE_AUDIO      0

void setup() {
  // Disable brown-out detector (helps prevent resets when SD writes)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  pinMode(BREAK_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  Serial.println("Starting setup...");

  // Flash LED twice to signal power on
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }

  // If not woke by break-wire, go to deep sleep waiting for launch
  if (!DANGEROUS_DEBUGGING_MODE && esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Waiting for launch (deep sleep)...");
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BREAK_PIN, 0);
    esp_deep_sleep_start();
  }

  // Woke from launch event
  Serial.println("Launch detected! Starting recording...");

  // Camera configuration
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
    config.frame_size   = FRAMESIZE_UXGA;    // max resolution if PSRAM present
    config.jpeg_quality = 15;                // Slightly lower quality for smaller files/faster writes (higher number = lower quality)
    config.fb_count     = 2;                 // double-buffering
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;    // fallback resolution
    config.jpeg_quality = 12;                // standard quality
    config.fb_count     = 1;                 // single buffer
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }
  config.grab_mode    = CAMERA_GRAB_LATEST;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
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

  // Initialize SD card
  // Mount SD in 1-bit mode so BREAK_PIN (GPIO12) is free
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card mount failed");
    return;
  }

  // Create a unique session folder using a random name and open MJPEG file
  uint32_t random_num = esp_random();
  char folder_name[12]; // '/' + 8 hex chars + null terminator
  snprintf(folder_name, sizeof(folder_name), "/%08X", random_num);
  String session_dir(folder_name);

  if (!SD_MMC.mkdir(session_dir.c_str())) {
    Serial.println("Failed to create session directory");
  }
  String mjpeg_path = session_dir + "/video.MJPEG";
  File mjpeg_file = SD_MMC.open(mjpeg_path.c_str(), FILE_WRITE);
  if (!mjpeg_file) {
    Serial.println("Failed to open MJPEG file");
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

  // Flash LED twice quickly to signal done
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }

  // Go back to deep sleep
  Serial.println("End of recording. Entering deep sleep...");
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BREAK_PIN, 0);
  esp_deep_sleep_start();
}

void loop() {
  // Not used
} 