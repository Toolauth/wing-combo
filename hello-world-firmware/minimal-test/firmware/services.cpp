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
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient https;

    // REDUCE TIMEOUT: 500ms is plenty for a local workshop server.
    // If it takes longer, the user should be moving to the Bypass Key anyway.
    https.setTimeout(500);

    https.begin(client, String(API_BASE_URL) + path);
    https.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["tool_id"] = TOOL_ID;
    doc[key] = value;

    String body;
    serializeJson(doc, body);
    int httpResponseCode = https.POST(body);
    https.end();

    return (httpResponseCode == 200); 
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


static String lastSeenUid = "";
static unsigned long lastScanAttempt = 0;
/*
Read RFID cards & Check authorization
>[!TIP] Generated with AI --> verify operation...
*/
bool processRfid() {
#if HAS_NFC
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
    uint8_t uidLength;

    // Fast glance (50ms timeout)
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
        
        // Build the Hex string for either 4-byte (Classic) or 7-byte (NTAG)
        String idString = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) idString += "0";
            idString += String(uid[i], HEX);
        }
        idString.toUpperCase();

        // Check cooldown (only for the same card)
        if (idString == lastSeenUid && (millis() - lastScanAttempt < 3000)) {
            return false; 
        }

        lastSeenUid = idString;
        lastScanAttempt = millis();

        // Attempt Network Auth
        // If server is down, this returns false immediately due to short timeout
        bool authorized = checkAuth(idString);
        
        if (authorized) {
            playAuthTone(); // Feedback in HAL
            return true;
        } else {
            playDenyTone(); // Feedback in HAL
            // LOG OFFLINE: If WiFi is down, we could save this attempt to SPIFFS here
        }
    }
#endif
    return false;
}

/*
Check if network is down, and try to reconnect without blocking
*/
void manageNetwork() {
    static unsigned long lastCheck = 0;
    if (millis() - lastCheck < 10000) return; // Check every 10s
    lastCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {
        // Non-blocking attempt to reconnect
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        // Do not use a while loop here! Let the next manageNetwork() check status.
    }
}