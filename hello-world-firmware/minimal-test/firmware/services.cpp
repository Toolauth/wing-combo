#include "hal.h"
#include "services.h"

void initNetwork() {
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Configure OTA
    ArduinoOTA.setHostname(TOOL_ID);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();
}

void handleOTA(){
    //pass
}

void enableTool(){
    setStatusLEDs(LOW, HIGH); //green
    digitalWrite(PIN_BUZZER, HIGH); //make noise
    setPreSetGate(HIGH); 
}
void unEnableTool(){
    setStatusLEDs(LOW, LOW); //dark
    digitalWrite(PIN_BUZZER, LOW); //quiet
    setPreSetGate(LOW); 
}

bool enable(bool relay, bool estop){
    if(relay){
        // tool must have turned ON
        return false;
    }
    if(estop){
        sendErrorStatus("enable requested, tool estop active");
        return false;
    }
    return true;
}

/*  Custom HTTP request to show status of more than one item.

*/
void sendHeartbeat() {
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat < 30*60*1000) return; // Every 30 minutes

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, String(API_BASE_URL) + "/heartbeat");
    https.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["tool_id"] = TOOL_ID;
    doc["uptime"] = millis() / 1000;
    doc["rssi"] = WiFi.RSSI();

    String body;
    serializeJson(doc, body);
    https.POST(body);
    https.end();
    lastHeartbeat = millis();
}

/*  Send a single item of data to a specified path.
    Return a Boolean true, only if a 200 reeponse.
    ALWAYS sends the `TOOL_ID` with data
*/

bool sendHttpData(String path, String key, String value){
    int httpResponseCode = 0;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;
    https.begin(client, String(API_BASE_URL) + path);
    https.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["tool_id"] = TOOL_ID;
    doc[key] = value;

    String body;
    serializeJson(doc, body);
    httpResponseCode = https.POST(body);
    https.end();
    //-------------------------------------------------
    // take action after server response
    if (httpResponseCode == 200) { // 200 is Authorized
        return true;
    }
    return false;
}


#if HAS_NFC
bool handleAuthFlow(uint8_t * uid, uint8_t uidLength){
    // ...magic to take RFID numbers to a string for auth
    char rfiduid[uidLength+2]; // Adjust size as needed
    rfiduid[0] = '\0'; // Initialize as empty string

    for (uint8_t i = 0; i < uidLength; i++) {
        sprintf(rfiduid + strlen(rfiduid), "%02X", uid[i]);
    }

    return sendHttpData( "/auth", "uid", String(rfiduid));
}
#endif

bool checkRemoteAuthorized(){
    //Should look to see if there are any actions needed from CC-server:
    // --> units without RFID need to authorize
    // --> ??? remote (CC origin) bypass
    return sendHttpData("/remote", "null", "null");
}

void checkTimeout(){
    bool should_reset = sendHttpData("/timeout", "null", "null");
    if (should_reset) { // 200 is confirmed
        triggerResetPulse();
    }
}

void sendEStopStatus(bool eStopState) {
    sendHttpData("/status", "estop", String(eStopState));
}

void sendBypassStatus(bool bypassState) {
    sendHttpData("/status", "bypass", String(bypassState));
}

void sendRelayStatus(bool relayState) {
    sendHttpData("/status", "relay", String(relayState));
}

void sendCurrentStatus(bool currentActive) {
    sendHttpData("/status", "current", String(currentActive));
}

void sendErrorStatus(String err) {
    sendHttpData("/error", "err", err);
}