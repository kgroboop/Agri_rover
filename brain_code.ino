/*
 * Agri-Rover Brain Node (ESP32-S3 N16R8 with OPI PSRAM & OV3660 Camera)
 * Complete & Finalized Version with Instant Pump Cut-off
 */

#define INCLUDE_OV3660 1
#include "esp_camera.h"
#include "img_converters.h"
#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESP32Servo.h>
#include <mbedtls/base64.h>
#include "DHT.h"

const char* WIFI_SSID = "AgriRover_Dashboard";
const char* WIFI_PASS = "12345678";
const char* phoneServerUrl = "http://192.168.4.3:5000/api/update";

/* ESP32-S3-CAM Pin Configuration ----------------------------------------- */
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4 
#define SIOC_GPIO_NUM     5 

#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11

#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

/* Sensor & Servo Pins ---------------------------------------------------- */
#define SOIL_PIN        1   
#define DHT_PIN         14  
#define SERVO_PROBE_PIN 41  
#define SERVO_PAN_PIN   2  
#define DHTTYPE         DHT22
#define SOIL_DRY_THRES  40  

const int SOIL_DRY_RAW = 4000; 
const int SOIL_WET_RAW = 500; 

enum State { SEARCHING, DRIVING_PHASE, SAMPLING_PAUSE };
State currentState = SEARCHING;

unsigned long phaseStartTime = 0;
unsigned long pumpStartTime = 0;
unsigned long lastDhtRead = 0;
bool pumpActive = false;
bool cycleLogged = false;

typedef struct struct_message {
  uint8_t driveCommand; 
  uint8_t motorSpeed;   
  bool pumpState;       
  float temperature;
  float humidity;
  int soilMoisture;
} struct_message;

struct_message controlData = {0, 0, false, 25.0, 60.0, 50};
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; 
esp_now_peer_info_t peerInfo;

DHT dht(DHT_PIN, DHTTYPE);
Servo probeServo;
Servo panServo;
WebServer camServer(80);

void handleSnapshot() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    camServer.send(504, "text/plain", "Camera capture failed");
    return;
  }
  uint8_t *buf = NULL;
  size_t len = 0;
  bool jpeg_converted = frame2jpg(fb, 80, &buf, &len);
  esp_camera_fb_return(fb);

  if (!jpeg_converted) {
    camServer.send(500, "text/plain", "JPEG compression failed");
    return;
  }
  camServer.sendHeader("Access-Control-Allow-Origin", "*");
  camServer.send_P(200, "image/jpeg", (const char *)buf, len);
  free(buf);
}

void onDataSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {}

bool detectBlueColor() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return false;

  uint32_t bluePixelCount = 0;
  uint32_t totalPixels = fb->width * fb->height;

  for (size_t i = 0; i < fb->len; i += 2) {
    uint16_t pixel = (fb->buf[i] << 8) | fb->buf[i + 1];
    uint8_t r = (pixel >> 11) & 0x1F; r = (r * 527 + 23) >> 6;
    uint8_t g = (pixel >> 5) & 0x3F;  g = (g * 259 + 33) >> 6;
    uint8_t b = pixel & 0x1F;         b = (b * 527 + 23) >> 6;

    if (b > 50 && (b - r) > 20 && (b - g) > 10) {
      bluePixelCount++;
    }
  }
  esp_camera_fb_return(fb);

  float blueRatio = (float)bluePixelCount / totalPixels;
  return (blueRatio > 0.10);
}

void logCycleWithPhoto(float temp, float hum, int soilPct, bool pump) {
  for (int i = 0; i < 2; i++) {
    camera_fb_t *stale = esp_camera_fb_get();
    if (stale) esp_camera_fb_return(stale);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) return;

  uint8_t *jpg_buf = NULL;
  size_t jpg_len = 0;
  if (!frame2jpg(fb, 80, &jpg_buf, &jpg_len)) {
    esp_camera_fb_return(fb);
    return;
  }
  esp_camera_fb_return(fb);

  size_t olen = 0;
  mbedtls_base64_encode(NULL, 0, &olen, jpg_buf, jpg_len);
  char *encoded_img = (char *)malloc(olen + 1);
  if (encoded_img) {
    mbedtls_base64_encode((unsigned char *)encoded_img, olen + 1, &olen, jpg_buf, jpg_len);
    encoded_img[olen] = '\0';

    HTTPClient http;
    http.begin(phoneServerUrl);
    http.addHeader("Content-Type", "application/json");
    
    String jsonPayload = "{\"soil\":" + String(soilPct) + 
                         ",\"temp\":" + String(temp, 1) + 
                         ",\"humidity\":" + String(hum, 1) + 
                         ",\"pumpState\":" + (pump ? "true" : "false") + 
                         ",\"image\":\"data:image/jpeg;base64," + String(encoded_img) + "\"}";

    http.POST(jsonPayload);
    http.end();
    free(encoded_img);
  }
  free(jpg_buf);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  probeServo.setPeriodHertz(50);
  panServo.setPeriodHertz(50);
  probeServo.attach(SERVO_PROBE_PIN, 600, 2400);
  panServo.attach(SERVO_PAN_PIN, 600, 2400);
  probeServo.write(180); 
  panServo.write(90);    

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  if (esp_now_init() != ESP_OK) return;
  esp_now_register_send_cb(onDataSent);
  
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; 
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_camera_init(&config);
  
  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_vflip(s, 1); 
    s->set_hmirror(s, 0);
  }

  camServer.on("/snapshot", HTTP_GET, handleSnapshot);
  camServer.begin();
}

void loop() {
  camServer.handleClient();

  // Non-blocking DHT read every 2 seconds
  if (millis() - lastDhtRead > 2000) {
    lastDhtRead = millis();
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    
    if (!isnan(t) && t > -40 && t < 80) controlData.temperature = t;
    if (!isnan(h) && h >= 0 && h <= 100) controlData.humidity = h;
  }

  int rawSoil = analogRead(SOIL_PIN);
  controlData.soilMoisture = constrain(map(rawSoil, SOIL_DRY_RAW, SOIL_WET_RAW, 0, 100), 0, 100);

  // Pump safety & instant wet cutoff logic
  if (pumpActive) {
    if (controlData.soilMoisture >= SOIL_DRY_THRES) {
      controlData.pumpState = false;
      pumpActive = false;
    } else if (millis() - pumpStartTime >= 3000) {
      controlData.pumpState = false;
      pumpActive = false;
    }
  }

  switch (currentState) {
    case SEARCHING: {
      cycleLogged = false; 
      controlData.driveCommand = 0; 
      controlData.motorSpeed = 0;
      controlData.pumpState = false;
      pumpActive = false;
      esp_now_send(broadcastAddress, (uint8_t *)&controlData, sizeof(controlData));
      
      if (detectBlueColor()) {
        phaseStartTime = millis();
        currentState = DRIVING_PHASE;
      }
      break;
    }
      
    case DRIVING_PHASE: {
      if (millis() - phaseStartTime < 2000) {
        controlData.driveCommand = 1; 
        controlData.motorSpeed = 160;
        controlData.pumpState = false;
        esp_now_send(broadcastAddress, (uint8_t *)&controlData, sizeof(controlData));
      } else {
        controlData.driveCommand = 0;
        controlData.motorSpeed = 0;
        esp_now_send(broadcastAddress, (uint8_t *)&controlData, sizeof(controlData));
        
        panServo.write(180);  // Pan Left
        probeServo.write(90);   // Lower Probe
        
        phaseStartTime = millis();
        currentState = SAMPLING_PAUSE;
      }
      break;
    }
      
    case SAMPLING_PAUSE: {
      controlData.driveCommand = 0;
      controlData.motorSpeed = 0;

      unsigned long elapsed = millis() - phaseStartTime;

      if (elapsed < 5000) {
        if (!pumpActive && !controlData.pumpState && controlData.soilMoisture < SOIL_DRY_THRES) {
          controlData.pumpState = true;
          pumpActive = true;
          pumpStartTime = millis();
        }

        if (!cycleLogged && elapsed > 2000) {
          logCycleWithPhoto(controlData.temperature, controlData.humidity, controlData.soilMoisture, controlData.pumpState);
          cycleLogged = true;
        }
      } else {
        probeServo.write(180);  // Retract Probe
        panServo.write(90);     // Reset Pan
        controlData.pumpState = false;
        pumpActive = false;
        currentState = SEARCHING;
      }
      
      esp_now_send(broadcastAddress, (uint8_t *)&controlData, sizeof(controlData));
      break;
    }
  }
  delay(50);
}