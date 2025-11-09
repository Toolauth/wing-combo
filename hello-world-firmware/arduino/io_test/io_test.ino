#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <ADE7953_I2C.h> // For Energy Monitor
#include <Rtttl.h>        // For Buzzer
#include <Adafruit_PN532.h> // For NFC

// --- WiFi Credentials (from YAML) ---
const char* ssid = "your_wifi_ssid";
const char* password = "your_wifi_password";
const char* ota_password = "d3281c2e8ad00b965c8bbb69717ce993";

// --- Pin Definitions (from YAML) ---
#define PIN_SDA 3
#define PIN_SCL 4
#define PIN_IRQ 11 // For ADE7953
#define PIN_RED_LED 5
#define PIN_GREEN_LED 6
#define PIN_RESET_RELAY 17
#define PIN_SET_RELAY 18
#define PIN_BUZZER 16
#define PIN_BYPASS_KEY 8
#define PIN_ESTOP_1 13
#define PIN_ESTOP_2 12
#define PIN_BIN_CUR 15
#define PIN_RELAY_FEEDBACK 9
// Note: PN532 I2C doesn't need extra pins beyond I2C

// --- Global Objects ---
WebServer server(80);
ADE7953 myADE7953(PIN_SCL, PIN_SDA); // Note: Uses custom I2C pins
Adafruit_PN532 nfc(PIN_SDA, PIN_SCL); // Uses default I2C pins
FLASH_STRING(song, "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6");

// --- Webpage Handlers ---
void handleRoot() {
  String html = "<html><body><h1>Wing Combo IO Test (Arduino)</h1>";
  html += "<p><a href='/sensors'>Get Sensor JSON</a></p>";
  html += "<h2>Outputs</h2>";
  html += "<p>LEDs: <a href='/led/red/on'>Red ON</a> | <a href='/led/red/off'>Red OFF</a> | <a href='/led/green/on'>Green ON</a> | <a href='/led/green/off'>Green OFF</a></p>";
  html += "<p>Relays: <a href='/relay/set/on'>Set ON</a> | <a href='/relay/set/off'>Set OFF</a> | <a href='/relay/reset/on'>Reset ON</a> | <a href='/relay/reset/off'>Reset OFF</a></p>";
  html += "<h2>Actions</h2>";
  html += "<p><a href='/nfc'>Scan NFC Tag (blocking)</a></p>";
  html += "<p><a href='/playmario'>Play Mario Song (blocking)</a></p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSensors() {
  // Read all sensors and format as JSON
  String json = "{";
  
  // Binary Sensors (All Active-LOW per YAML)
  json += "\"bypass_key\": " + String(!digitalRead(PIN_BYPASS_KEY));
  json += ", \"estop_1\": " + String(!digitalRead(PIN_ESTOP_1));
  json += ", \"estop_2\": " + String(!digitalRead(PIN_ESTOP_2));
  json += ", \"bin_cur\": " + String(!digitalRead(PIN_BIN_CUR));
  json += ", \"relay_feedback\": " + String(!digitalRead(PIN_RELAY_FEEDBACK));
  
  // ADE7953 Sensors
  json += ", \"ade7953\": {";
  json += "\"voltage\": " + String(myADE7953.getVrms()); // getVrms(), getIrmsA(), etc.
  json += ", \"current_a\": " + String(myADE7953.getIrmsA());
  json += ", \"current_b\": " + String(myADE7953.getIrmsB());
  json += ", \"active_power_a\": " + String(myADE7953.getActivePowerA());
  json += "}";
  
  json += "}";
  server.send(200, "application/json", json);
}

void handleNFC() {
  String msg = "NFC Scan: ";
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
  uint8_t uidLength;

  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100); // 100ms timeout

  if (success) {
    msg += "Tag found! UID: ";
    for (uint8_t i = 0; i < uidLength; i++) {
      msg += String(uid[i], HEX);
      msg += " ";
    }
  } else {
    msg += "No tag found.";
  }
  server.send(200, "text/plain", msg);
}

void handlePlayMario() {
  server.send(200, "text/plain", "Playing Mario...");
  Rtttl::play(song);
}

void handleOutput() {
  // Handle /led/red/on, /relay/set/off, etc.
  String target = server.pathArg(0);
  String state = server.pathArg(1);
  int pin = -1;
  
  if (target == "red") pin = PIN_RED_LED;
  if (target == "green") pin = PIN_GREEN_LED;
  if (target == "set") pin = PIN_SET_RELAY;
  if (target == "reset") pin = PIN_RESET_RELAY;
  
  if (pin != -1) {
    digitalWrite(pin, (state == "on") ? HIGH : LOW);
    handleRoot(); // Send back to main page
  } else {
    server.send(404, "text/plain", "Not Found");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Booting network-enabled test...");

  // --- Initialize Outputs ---
  pinMode(PIN_RED_LED, OUTPUT);
  pinMode(PIN_GREEN_LED, OUTPUT);
  pinMode(PIN_RESET_RELAY, OUTPUT);
  pinMode(PIN_SET_RELAY, OUTPUT);

  // --- Initialize Inputs (Matching YAML) ---
  // All are Active-LOW. pullup only on relay_feedback.
  pinMode(PIN_BYPASS_KEY, INPUT);
  pinMode(PIN_ESTOP_1, INPUT);
  pinMode(PIN_ESTOP_2, INPUT);
  pinMode(PIN_BIN_CUR, INPUT);
  pinMode(PIN_RELAY_FEEDBACK, INPUT_PULLUP);

  // --- Initialize I2C and Peripherals ---
  // Wire.begin(PIN_SDA, PIN_SCL); // Standard I2C init
  myADE7953.initialize(); // ADE library handles its own I2C
  nfc.begin();
  
  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.println("Didn't find PN532 board");
  } else {
    Serial.print("Found PN532! Firmware ver. "); 
    Serial.print((versiondata >> 24) & 0xFF, HEX);
    Serial.print("."); 
    Serial.println((versiondata >> 16) & 0xFF, HEX);
  }

  // --- Connect to WiFi ---
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());

  // --- Setup Web Server Routes ---
  server.on("/", handleRoot);
  server.on("/sensors", handleSensors);
  server.on("/nfc", handleNFC);
  server.on("/playmario", handlePlayMario);
  // Route for /led/red/on, /relay/set/off, etc.
  server.on("/(led|relay)/(red|green|set|reset)/(on|off)", handleOutput);
  
  server.begin();

  // --- Setup OTA (matches YAML) ---
  ArduinoOTA.setHostname("wing-combo-io-test");
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();

  Serial.println("Ready. Visit IP address in browser.");
}

void loop() {
  ArduinoOTA.handle();   // Handle OTA requests
  server.handleClient(); // Handle web requests
  Rtttl::updateMelody(); // Update non-blocking RTTTL player
}