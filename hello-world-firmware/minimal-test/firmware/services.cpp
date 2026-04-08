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
Custom HTTP request to show status of more than one item.

- TOOL_ID
- uptime
- rssi
*/
void sendHeartbeat() {
    if (WiFi.status() != WL_CONNECTED) return; //fail without blocking

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
RETURN: void --> side effect: RESET tool latch
*/
void checkTimeout(){
    if (WiFi.status() != WL_CONNECTED) return; //fail without blocking

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
        digitalWrite(PIN_BUZZER, HIGH); //noise
        bool response = sendHttpData( "/auth", "uid", idString);
        digitalWrite(PIN_BUZZER, LOW);  //quiet

        return response;
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


/*
Single source of truth for LED refresh
*/
void updateUI(bool estop, bool bypass, bool authorized) {
    if (estop) {
        setStatusLEDs(HIGH, LOW); // Solid Red for E-Stop
    } else if (bypass || authorized) {
        setStatusLEDs(LOW, HIGH); // Green for Active
    } else {
        setStatusLEDs(LOW, LOW);  // Dark for Safe
    }
}