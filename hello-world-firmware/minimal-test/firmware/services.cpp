#include "hal.h"
#include "services.h"

/* 
Setup WiFi and Prepare for OTA updates
*/
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

/*
Bare metal controls for a simple 'enable' window start.
This is opens a "pre-Set" for the self latching relay.
*/
void enableTool(){
    setStatusLEDs(LOW, HIGH); //green
    digitalWrite(PIN_BUZZER, HIGH); //make noise
    setPreSetGate(HIGH); 
}

/*
Bare metal controls for a simple 'enable' window stop.
This is finishes a "pre-Set" for the self latching relay.
*/
void unEnableTool(){
    setStatusLEDs(LOW, LOW); //dark
    digitalWrite(PIN_BUZZER, LOW); //quiet
    setPreSetGate(LOW); 
}

/*
Flailing with this one --> messy...
*/
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

/*
Custom HTTP request to show status of more than one item.

- TOOL_ID
- uptime
- rssi
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

/*  
Send arbitrary JSON key/value to specified path of API.

INPUT: server path, key/value data to send in `http.POST`
- ALWAYS sends the `TOOL_ID` alongside data key/value pair

RETURN: true, if a 200 response | false
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
/*
>[!WARNING] confirm output of Adafruit PN532 lib
---
INPUT: RFID number as a list of numbers
- ...magic to take RFID numbers to a string for auth
- Make auth request for RFID number
RETURN: `true`, if a 200 response | `false`
*/
bool handleAuthFlow(uint8_t * uid, uint8_t uidLength){
    // 
    char rfiduid[uidLength+2]; // Adjust size as needed
    rfiduid[0] = '\0'; // Initialize as empty string

    for (uint8_t i = 0; i < uidLength; i++) {
        sprintf(rfiduid + strlen(rfiduid), "%02X", uid[i]);
    }

    return sendHttpData( "/auth", "uid", String(rfiduid));
}
#endif


/*
Ask if server wants to authorize the tool.
Sends `TOOL_ID` & `null` to /remote path.
RETURN: true, if a 200 response | false
*/
bool checkRemoteAuthorized(){
    return sendHttpData("/remote", "null", "null");
}


/*
Ask if server wants to timeout abandoned tool.
Sends `TOOL_ID` & `null` to /timeout path.
RETURN: true, if a 200 response | false
*/
void checkTimeout(){
    bool should_reset = sendHttpData("/timeout", "null", "null");
    if (should_reset) { // 200 is confirmed
        triggerResetPulse();
    }
}

/*
Send status-change event to server.
Sends `TOOL_ID` & `eStopState` to /status path.
> Does not handle HTTP response at all.
*/
void sendEStopStatus(bool eStopState) {
    sendHttpData("/status", "estop", String(eStopState));
}

/*
Send status-change event to server.
Sends `TOOL_ID` & `bypassState` to /status path.
> Does not handle HTTP response at all.
*/
void sendBypassStatus(bool bypassState) {
    sendHttpData("/status", "bypass", String(bypassState));
}

/*
Send status-change event to server.
Sends `TOOL_ID` & `relayState` to /status path.
> Does not handle HTTP response at all.
*/
void sendRelayStatus(bool relayState) {
    sendHttpData("/status", "relay", String(relayState));
}

/*
Send status-change and/or heartbeat to server.
Sends `TOOL_ID` & `currentActive` to /status path.
> Does not handle HTTP response at all.
*/
void sendCurrentStatus(bool currentActive) {
    sendHttpData("/status", "current", String(currentActive));
}

/*
Send error event to server.
Sends `TOOL_ID` & error-string to /status path.
> Does not handle HTTP response at all.
*/
void sendErrorStatus(String err) {
    sendHttpData("/error", "err", err);
}