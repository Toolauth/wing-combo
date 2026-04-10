#include "hal.h"
#include "services.h"
#include "watchdog.h"

unsigned long lastSuccessfulCommunication = 0;

/* 
Setup WiFi and Prepare for OTA updates
*/
void initNetwork() {
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    rebootCounter();
    
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

    if (WiFi.status() == WL_CONNECTED) {
        // If we have been connected and successful for a while, 
        // consider the "reboot cycle" broken.
        extern unsigned long lastSuccessfulCommunication;
        if (millis() - lastSuccessfulCommunication < 5000) { 
            // We just had a successful talk! Reset the tally.
            bootCount = 0; 
        }
    } else {
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        selfSoftwareRestart();
    }
}

/*  
Send arbitrary JSON key/value to specified path of API.

INPUT: server path, key/value data to send in `http.POST`
- ALWAYS sends the `TOOL_ID` alongside data key/value pair

RETURN: true, if a 200 response | false
*/
bool sendHttpData(String path, String key, String value, unsigned long offsetMs){
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
    doc["offset_ms"] = offsetMs;

    String body;
    serializeJson(doc, body);
    int httpResponseCode = https.POST(body);
    https.end();

    if (httpResponseCode == 200) {
        lastSuccessfulCommunication = millis(); // Update on actual success
        return true;
    }
    return false;
}    

std::deque<NetworkEvent> eventQueue;
const size_t MAX_QUEUE_SIZE = 30;
const size_t PRESSURE_THRESHOLD = 20;
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
    // Calculate how long ago this happened
    unsigned long ageMs = millis() - event.queuedAt;

    if (sendHttpData(event.path, event.key, event.value, ageMs)) {
        eventQueue.pop_front(); // Only remove if successfully sent
        lastSend = millis();
    }
}

/*
Create queue item, and add it to the queue.
*/
void queueEvent(String path, String key, String value) {
    bool isHeartbeat = (path == "/heartbeat");

    // 1. Coalescing Logic: Look for an existing pending update for this specific key
    // We search backwards (from back to front) to find the most recent matching event
    for (auto it = eventQueue.rbegin(); it != eventQueue.rend(); ++it) {
        if (it->path == path && it->key == key) {
            it->value = value; // Update the value in place
            it->queuedAt = millis(); // Update the time too!
            return;            // Exit early - we've coalesced the event
        }
    }

    // 2. Queue Pressure Management (for new events)
    if (isHeartbeat && eventQueue.size() > PRESSURE_THRESHOLD) {
        return; 
    }

    // 3. If strictly full, drop the oldest (FIFO)
    if (eventQueue.size() >= MAX_QUEUE_SIZE) {
        eventQueue.pop_front();
    }

    // 4. Add the new unique event
    eventQueue.push_back({path, key, value, millis()});
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

/**
 * Helper to debounce signals, send status updates, and handle "Still-On" heartbeats.
 * @param current: The live reading from the sensor
 * @param last: Reference to the 'last' state in your ToolState struct
 * @param dTimer: Reference to the debounce timer
 * @param hTimer: Reference to the heartbeat timer (for 30s updates)
 * @param key: The JSON key for reporting (e.g., "estop")
 * @param useHeartbeat: If true, sends updates every 30s while 'current' is true
 */
void monitorSignal(bool current, bool &last, unsigned long &dTimer, unsigned long &hTimer, const char* key, bool useHeartbeat) {
    // 1. Handle State Changes (Debounced)
    if (current != last) {
        if (millis() - dTimer > 50) { // 50ms Debounce
            last = current;
            queueEvent("/status", key, String(current));
            if (useHeartbeat) hTimer = millis(); // Reset heartbeat on state change
        }
    } else {
        dTimer = millis(); // Keep timer current as long as state is stable
    }

    // 2. Handle "Still-Active" Heartbeats
    if (useHeartbeat && current && (millis() - hTimer > 30000)) {
        queueEvent("/status", key, String(current));
        hTimer = millis();
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

    bool should_reset = sendHttpData("/timeout", "null", "null", 0);
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
    return sendHttpData("/remote", "null", "null", 0);
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
