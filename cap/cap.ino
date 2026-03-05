#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#define CAMERA_GRAB_LATEST (camera_grab_mode_t)1

// ===================== CONFIG =====================
const char* ssid = "Xi Quach";
const char* password = "handoan1";
String SERVER_IP = "10.207.165.139";
String CON2_IP   = "10.207.165.172";
// Python server:
String serverUrl = "http://" + SERVER_IP + ":5000/upload";   // Đổi IP cho đúng

// Cảm biến cửa (ví dụ dùng Reed Switch)
#define SENSOR_PIN 2   // CHỌN CHÂN BẠN ĐANG DÙNG

int lastState = HIGH;       // trạng thái trước đó

unsigned long lastActionTime = 0;
const unsigned long PERIOD_30S = 1800000;   // 30,000 ms = 30s
bool doorEvent = false;   // có open/close chen giữa hay chưa


// ESP32-S3-CAM Pins
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13


void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  lastState = digitalRead(SENSOR_PIN);

  // ================== CAMERA CONFIG ==================
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
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

  config.frame_size   = FRAMESIZE_VGA;
  config.jpeg_quality = 10;
  config.fb_count     = 2;
  config.grab_mode = CAMERA_GRAB_LATEST;  // luôn lấy frame mới nhất

  // Init camera
  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init FAILED");
    return;
  }

  // ============ WIFI ===================
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected!");
}

void sendImage(String imgType) {
  camera_fb_t *fb = esp_camera_fb_get();
  if (fb) esp_camera_fb_return(fb);

  // đợi camera cập nhật frame mới
  delay(300);

  // CHỤP ẢNH MỚI THẬT SỰ
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Failed to capture image");
    return;
  }

  Serial.println("📤 Sending image to server...");

  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "image/jpeg");
  http.addHeader("X-Image-Type", imgType);   

  int httpResponseCode = http.POST(fb->buf, fb->len);

  if (httpResponseCode > 0) {
    Serial.printf("✅ Upload OK, response: %d\n", httpResponseCode);
  } else {
    Serial.printf("❌ Upload failed: %s\n", http.errorToString(httpResponseCode).c_str());
  }

  http.end();
  esp_camera_fb_return(fb);
}

void sendDoorState(String state) {
  WiFiClient client;

  if (client.connect(CON2_IP.c_str(), 80)) {   // 🔴 ĐẶT IP CON_2 Ở ĐÂY
    client.print("GET /door?state=");
    client.print(state);
    client.println(" HTTP/1.1");
    client.println("Host: esp32");
    client.println("Connection: close");
    client.println();
    Serial.printf("🚪 Sent door state → %s\n", state.c_str());
  } else {
    Serial.println("❌ Failed to connect to CON_2");
  }
}

void sendSignalToCon2() {
  HTTPClient http;
  WiFiClient client;

  // LED ON
  http.begin(client, "http://" + CON2_IP + "/led?state=on");
  http.GET();
  http.end();

  Serial.println("LED ON for 1s");
  
  sendImage("auto");   
  // LED OFF
  http.begin(client, "http://" + CON2_IP + "/led?state=off");
  http.GET();
  http.end();

  Serial.println("LED OFF");
}




void loop() {
  int currentState = digitalRead(SENSOR_PIN);
  delay(80); 
  if (digitalRead(SENSOR_PIN) != currentState) return; // chống nhiễu

  if (currentState != lastState) {
    doorEvent = true; // ghi nhận là đã có sự kiện cửa chen vào
    String type = (currentState == HIGH) ? "open" : "close";
    sendDoorState(type);
    if (type == "open") {
      Serial.println("💡 Door OPEN → delay 1s cho LED sáng đủ");
      delay(1000);
    }
    Serial.printf("📸 Trạng thái thay đổi → chụp ảnh (%s)\n", type.c_str());

    camera_fb_t *fb = esp_camera_fb_get();
    
    if (!fb) {
      Serial.println("❌ Camera capture failed");
    } else {
      Serial.println("📤 Sending image to server...");

      WiFiClient client;
      if (client.connect(SERVER_IP.c_str(), 5000)) {

        // ---- HTTP POST Header ----
        client.println("POST /upload HTTP/1.1");
        client.print("Host: ");
        client.println(SERVER_IP);
        client.println("Content-Type: image/jpeg");
        client.print("X-Image-Type: ");
        client.println(type);     // <---- GỬI OPEN / CLOSE
        client.print("Content-Length: ");
        client.println(fb->len);
        client.println();

        // ---- Gửi ảnh ----
        client.write(fb->buf, fb->len);
        client.stop();

        Serial.printf("✔ Image (%s) sent.\n", type.c_str());

      } else {
        Serial.println("❌ Upload failed");
      }

      esp_camera_fb_return(fb);
    }

    lastState = currentState;
  }
    unsigned long now = millis();

  if (now - lastActionTime >= PERIOD_30S) {
    lastActionTime = now;

    Serial.println("⏱ AUTO cycle: LED ON → capture → LED OFF");

    sendSignalToCon2();   // ⭐ bật LED + chụp + gửi server (auto)
    
    if (doorEvent) {
      Serial.println("AUTO after door event → will compare with CLOSE");
      doorEvent = false;
    } else {
      Serial.println("AUTO normal → compare with previous AUTO");
    }
  }
  delay(50);
}
