#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

const char* ssid = "Xi Quach";
const char* password = "handoan1";

WebServer server(80);

// -------- Relay chính (JSON) ----------
const int relayPins[1] = {26};
const int NUM_RELAYS = 1;

bool doorOpen = false;
bool buzzerActive = false;  // trạng thái buzzer, toàn cục
// -------- Relay phụ ----------
const int LED_RELAY_PIN = 25;   // 👉 dãy lED 
const int LOCK_RELAY_PIN  = 27;   // 👉 khóa đẩy
const int BUZZER_PIN = 33;   // 🔔 buzzer

const bool ACTIVE_LOW = false;

unsigned long relayStartTimes[NUM_RELAYS];
bool relayActive[NUM_RELAYS];

// ===================================================
void setRelay(int index, bool state) {
  if (index < 0 || index >= NUM_RELAYS) return;

  digitalWrite(relayPins[index], ACTIVE_LOW ? !state : state);
  relayActive[index] = state;

  if (state) relayStartTimes[index] = millis();

  Serial.printf("Relay MAIN (26) -> %s\n", state ? "ON" : "OFF");

  // LOCK relay cùng trạng thái 26
  digitalWrite(LED_RELAY_PIN, ACTIVE_LOW ? !state : state);

  // LED relay NGƯỢC trạng thái 26
  bool opposite = !state;
  digitalWrite(LOCK_RELAY_PIN , ACTIVE_LOW ? !opposite : opposite);

  if (state && buzzerActive) {
    digitalWrite(BUZZER_PIN, LOW); // relay bật → tắt buzzer
    buzzerActive = false;
  }
  Serial.printf("Relay LED -> %s\n", state ? "ON" : "OFF");
  Serial.printf("Relay LOCK  -> %s\n", opposite ? "ON" : "OFF");
}


// ===================================================
void handleRelay() {
  if (!server.hasArg("plain")) {
    server.send(400, "application/json", "{\"error\":\"Missing JSON body\"}");
    return;
  }

  String body = server.arg("plain");
  StaticJsonDocument<256> doc;

  if (deserializeJson(doc, body)) {
    server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
    return;
  }

  int relayIndex = doc["relay"];
  String state = doc["state"] | "off";

  if (relayIndex < 0 || relayIndex >= NUM_RELAYS) {
    server.send(400, "application/json", "{\"error\":\"Relay index out of range\"}");
    return;
  }

  bool turnOn = (state == "on");
  setRelay(relayIndex, turnOn);

  // ❗ JSON chỉ báo relay 26 như cũ
  String response = "{\"ok\":true,\"relay\":" + String(relayIndex) +
                    ",\"state\":\"" + state + "\"}";
  server.send(200, "application/json", response);
}

void handleLED() {
  if (!server.hasArg("state")) {
    server.send(400, "text/plain", "Missing state");
    return;
  }

  String state = server.arg("state");

  if (state == "on") {
    digitalWrite(LED_RELAY_PIN, HIGH);
    server.send(200, "text/plain", "LED ON");
  }
  else if (state == "off") {
    digitalWrite(LED_RELAY_PIN, LOW);
    server.send(200, "text/plain", "LED OFF");
  }
  else {
    server.send(400, "text/plain", "Invalid state");
  }
}

void handleDoorState() {
  if (!server.hasArg("state")) {
    server.send(400, "text/plain", "Missing state");
    return;
  }

  String state = server.arg("state");

  // ===================== DOOR OPEN =====================
  if (state == "open") {

    doorOpen = true;

    bool relay26CurrentlyOn =
      (digitalRead(relayPins[0]) == (ACTIVE_LOW ? LOW : HIGH));

    if (!relay26CurrentlyOn) {
      Serial.println("🔔 Buzzer: door opened while relay 26 was OFF");
      beepBuzzer();
      buzzerActive = true; // đánh dấu đang hú
    }

    server.send(200, "text/plain", "Door OPEN checked relay + buzzer logic");
    return;
  }

  //   // relay 26 ON
  //   digitalWrite(relayPins[0], HIGH);

  //   // LED ON ngay
  //   digitalWrite(LED_RELAY_PIN, HIGH);

  //   // LOCK OFF
  //   digitalWrite(LOCK_RELAY_PIN, LOW);

  //   server.send(200, "text/plain", "Door OPEN → 26 ON, LED ON, LOCK OFF");
  // }

  // ===================== DOOR CLOSE =====================
  if (state == "close") {
  doorOpen = false;

  // relay 26 OFF ngay
  digitalWrite(relayPins[0], LOW);

  // LED OFF ngay
  digitalWrite(LED_RELAY_PIN, LOW);

  // LOCK ON ngay
  digitalWrite(LOCK_RELAY_PIN, HIGH);

  digitalWrite(BUZZER_PIN, LOW); // tắt buzzer
  buzzerActive = false;

  server.send(200, "text/plain", "Door CLOSE → 26 OFF, LED OFF, LOCK ON");
}


  else {
    server.send(400, "text/plain", "Invalid state");
  }
}

void handleLedOn() {
  digitalWrite(LED_RELAY_PIN, HIGH);   // bật LED 25
  server.send(200, "text/plain", "LED 25 ON");
  Serial.println("LED 25 turned ON by CAP");
}

void beepBuzzer() {
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(LED_RELAY_PIN, HIGH); // 💡 bật LED chân 25
  Serial.println("🔔 Buzzer ON (door open while relay 26 OFF)");
}


// ===================================================
void setup() {
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println(WiFi.localIP());
  // relay chính
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], ACTIVE_LOW ? HIGH : LOW);
    relayActive[i] = false;
  }

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // relay LED
  pinMode(LED_RELAY_PIN, OUTPUT);
  digitalWrite(LED_RELAY_PIN, ACTIVE_LOW ? HIGH : LOW);

  // relay LOCK
  pinMode(LOCK_RELAY_PIN, OUTPUT);
  digitalWrite(LOCK_RELAY_PIN, ACTIVE_LOW ? HIGH : LOW);

  server.on("/relay", HTTP_POST, handleRelay);
  server.on("/door", handleDoorState);
  server.on("/led", HTTP_GET, handleLED);

  server.begin();
}

// ===================================================
void loop() {
  server.handleClient();

  unsigned long now = millis();
}
