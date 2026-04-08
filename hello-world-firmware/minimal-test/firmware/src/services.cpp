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
    https.setTimeout(500); //fail fast if the server is slow

    https.begin(client, String(API_BASE_URL) + path);
    https.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["tool_id"] = TOOL_ID;
    doc[key] = value;

    String body;
    serializeJson(doc, body);
    int httpResponseCode = https.POST(body);
    https.end();

    return (httpResponseCode == 200); 
}    

std::queue<NetworkEvent> eventQueue;
/*
Send items in http queue.

- max 5 http items per second
- max queue of 30 items
- down network: try again when its back up
*/
void processNetworkQueue() {
    if (eventQueue.empty() || WiFi.status() != WL_CONNECTED) return;

    static unsigned long lastSend = 0;
    if (millis() - lastSend < 200) return; // Rate limit

    NetworkEvent event = eventQueue.front();
    if (sendHttpData(event.path, event.key, event.value)) {
        eventQueue.pop(); // Only remove if successfully sent
        lastSend = millis();
    }
}

/*
Create queue item, and add it to the queue.
*/
void queueEvent(String path, String key, String value) {
    if (eventQueue.size() < 30) { // Limit queue size to prevent memory issues
        eventQueue.push({path, key, value});
    }
}


//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
/*
Queue heartbeat items every 30 minutes.

- uptime
- rssi
*/
void sendHeartbeat() {
    if (WiFi.status() != WL_CONNECTED) return; //fail without blocking

    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat < 30*60*1000) return; // Every 30 minutes

    queueEvent("/heartbeat", "uptime", String(millis() / 1000));
    queueEvent("/heartbeat", "rssi", String(WiFi.RSSI()));
    
    lastHeartbeat = millis();
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
Ask if server wants to authorize the tool.
Sends `TOOL_ID` & `null` to /remote path.
RETURN: true, if a 200 response | false
*/
bool checkRemoteAuthorized(){
    return sendHttpData("/remote", "null", "null");
}    



static String lastSeenUid = "";
static unsigned long lastScanAttempt = 0;
/*
Read RFID cards & Check authorization
>[!TIP] Generated with AI --> verify operation...
*/
void processRfid() {
#if HAS_NFC
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
    uint8_t uidLength;

    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
        String idString = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) idString += "0";
            idString += String(uid[i], HEX);
        }
        idString.toUpperCase();

        if (idString == lastSeenUid && (millis() - lastScanAttempt < SCAN_COOLDOWN)) return;

        lastSeenUid = idString;
        lastScanAttempt = millis();

        // 1. Instant feedback (No lag!)
        digitalWrite(PIN_BUZZER, HIGH);
        delay(50); // Minimal delay for physical feedback
        digitalWrite(PIN_BUZZER, LOW);

        // 2. Fire and Forget (Tell the server, don't wait for permission)
        queueEvent("/auth", "uid", idString);
    }
#endif 
}
