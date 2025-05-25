
#include "esp_camera.h"
#include "esp_timer.h"
#include "Arduino.h"
// #include "FS.h"                // SD Card ESP32
// #include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>  // read and write from flash memory
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFi.h>

// define the number of bytes you want to access
// #define EEPROM_SIZE 1

// Pin definition for CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27

#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

// int pictureNumber = 0;

const char* ssid = "ssid";
const char* password = "password";
const char* post_url = "https://your_deployed_url/api/upload/";  // Location where images are POSTED
bool internet_connected = false;

#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  //disable brownout detector

  Serial.begin(115200);
  //Serial.setDebugOutput(true);
  //Serial.println();


  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS Mount failed.");
    return;
  }

  // initialize EEPROM with predefined size


  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_QVGA;  // FRAMESIZE_ + QVGA|CIF|VGA|SVGA|XGA|SXGA|UXGA
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Init Camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    return;
  }

  //Serial.println("Starting SD Card");
  // if (!SD_MMC.begin())
  // {
  //   Serial.println("SD Card Mount Failed");
  //   return;
  // }

  // uint8_t cardType = SD_MMC.cardType();
  // if (cardType == CARD_NONE)
  // {
  //   Serial.println("No SD Card attached");
  //   return;
  // }
}
void captureImage() {
  //capture image

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }


  // Path where new picture will be saved in SD Card
  String timestamp = String(millis());
  String path = "/picture_" + timestamp + ".jpg";

  // fs::FS &fs = SD_MMC;
  Serial.printf("Picture file name: %s\n", path.c_str());



  File file = SPIFFS.open(path.c_str(), FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file in writing mode");
  } else {
    file.write(fb->buf, fb->len);  // payload (image), payload length
    Serial.printf("Saved file to path: %s\n", path.c_str());
  }
  sendImageToServer(fb, timestamp);
  file.close();
  esp_camera_fb_return(fb);


  // Turns off the ESP32-CAM white on-board LED (flash) connected to GPIO 4
  // pinMode(4, OUTPUT);
  // digitalWrite(4, LOW);
  // rtc_gpio_hold_en(GPIO_NUM_4);
  delay(2000);
  Serial.println("Going to sleep now");
  delay(2000);
  esp_deep_sleep_start();
  Serial.println("This will never be printed");
}
void sendImageToServer(camera_fb_t* fb, String timestamp) {
  HTTPClient http;
  Serial.print("[HTTP] begin...\n");

  // First attempt at sending the request
  http.begin(post_url);  // Use the initial post_url defined in your code
  http.addHeader("Content-Type", "multipart/form-data; boundary=boundary");
  Serial.print("[HTTP] POST...\n");
  // Prepare the body with multipart data
  String body = "------boundary\r\n";
  body += "Content-Disposition: form-data; name=\"uploaded_images\"; filename=\"picture_" + timestamp + ".jpg\"\r\n";
  body += "Content-Type: image/jpeg\r\n\r\n";

  // Append the image data
  for (size_t i = 0; i < fb->len; i++) {
    body += (char)fb->buf[i];
  }

  body += "\r\n------boundary--\r\n";

  // Send the POST request
  int httpCode = http.POST(body);

  // Handle the response
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("Server Response: " + payload);
  } else {
    Serial.printf("[HTTPS] POST failed, error: %s\n", http.errorToString(httpCode).c_str());
  }


  // End the HTTP session
  http.end();
}



void loop() {
  if (init_wifi()) {  // Connected to WiFi
    internet_connected = true;
    Serial.println("Internet connected");
    captureImage();
  }
}



bool init_wifi() {
  Serial.printf("Connecting to %s\n", ssid);
  WiFi.begin(ssid, password);
  for (int i = 0; i < 10; i++) {
    if (WiFi.status() == WL_CONNECTED) return true;
    delay(500);
    Serial.print(".");
  }
  Serial.println("Failed to connect to WiFi");
  return false;
}