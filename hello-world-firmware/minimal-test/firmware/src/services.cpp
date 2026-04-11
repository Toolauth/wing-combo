#include "services.h"
#include "hal.h"
#include "watchdog.h"

WiFiClient espClient;
PubSubClient client(espClient);
std::deque<NetworkEvent> eventQueue;
//--------------------------------------
// Networking & MQTT
/* 
Setup WiFi and Prepare for OTA updates
*/
void initNetwork() {
    WiFi.setSleep(false);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    client.setServer(MQTT_SERVER, 1883);
    client.setCallback(mqttCallback);

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
Send items in MQTT Queue
*/
void processNetworkQueue() {
    if (!client.connected()) {
        if (!mqttReconnect()) return;
    }
    client.loop(); // Handle incoming messages
    
    if (eventQueue.empty()) return;
    
    static unsigned long lastSend = 0;
    if (millis() - lastSend < 150) return; // Rate limit
    
    NetworkEvent event = eventQueue.front();
    
    // Create JSON with Time-Travel Offset
    JsonDocument doc;
    doc["val"] = event.value;
    doc["offset_ms"] = millis() - event.queuedAt;
    
    char buffer[128];
    serializeJson(doc, buffer);
    
    if (client.publish(event.topic.c_str(), buffer)) {
        eventQueue.pop_front();
        lastSend = millis();
    }
}

void queueEvent(String subPath, String value) {
    String fullTopic = "stat/" + String(TOOL_ID) + "/" + subPath;
    
    // --- COALESCING LOGIC ---
    // Search the queue: if this topic is already pending, just update it    
    for (auto &event : eventQueue) {
        if (event.topic == fullTopic) {
            event.value = value;
            event.queuedAt = millis();
            return;
        }
    }
    
    // --- QUEUE PRESSURE ---
    if (eventQueue.size() >= 30) eventQueue.pop_front();
    
    // Add new event
    eventQueue.push_back({fullTopic, value, millis()});
}

//-----------------------------------------------
// MQTT Specifics

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String msg = "";
    for (int i = 0; i < length; i++) msg += (char)payload[i];
    
    String tool = String(TOOL_ID);

    // 1. Handling the Enable/Disable Switch
    if (String(topic) == "cmnd/" + tool + "/enable") {
        if (msg == "ON") enableTool();
        else unEnableTool(); // This is the physical RESET pulse
    }

    // 2. Handling a Direct Reset Command (Matching the YAML's intent)
    // This gives your colleague a dedicated "Pulse Reset" button/action
    if (String(topic) == "cmnd/" + tool + "/reset") {
        triggerResetPulse();
        unEnableTool(); 
        // We also report back that authorization is now dead
        queueEvent("enabled", "OFF"); 
    }
}

void sendDiscovery() {
    String tool = String(TOOL_ID);

    // 1. RFID Sensor Discovery
    String rfidConfigTopic = "homeassistant/sensor/" + tool + "/rfid/config";
    String rfidPayload = "{\"name\":\"" + tool + " RFID\", \"stat_t\":\"stat/" + tool + "/rfid\", \"val_tpl\":\"{{ value_json.uid }}\"}";
    client.publish(rfidConfigTopic.c_str(), rfidPayload.c_str(), true);

    // 2. Enable Switch Discovery (The "SET" Relay)
    String switchConfigTopic = "homeassistant/switch/" + tool + "/enable/config";
    String switchPayload = "{\"name\":\"" + tool + " Enable\", \"stat_t\":\"stat/" + tool + "/state\", \"cmd_t\":\"cmnd/" + tool + "/enable\"}";
    client.publish(switchConfigTopic.c_str(), switchPayload.c_str(), true);
    
    // Example: The E-Stop Binary Sensor
    // We add "value_template" so HA ignores the offset_ms and just looks at "val"
    String estopTopic = "homeassistant/binary_sensor/" + tool + "/estop/config";
    String estopPayload = "{"
        "\"name\":\"" + tool + " E-Stop\","
        "\"stat_t\":\"stat/" + tool + "/estop\","
        "\"val_tpl\":\"{{ value_json.val }}\"," 
        "\"dev_cla\":\"problem\""
    "}";
    client.publish(estopTopic.c_str(), estopPayload.c_str(), true);

    // Example: The E-Stop Binary Sensor
    // We add "value_template" so HA ignores the offset_ms and just looks at "val"
    String bypassTopic = "homeassistant/binary_sensor/" + tool + "/bypass/config";
    String bypassPayload = "{"
        "\"name\":\"" + tool + " Bypass Key\","
        "\"stat_t\":\"stat/" + tool + "/bypass\","
        "\"val_tpl\":\"{{ value_json.val }}\"," 
        "\"dev_cla\":\"problem\""
    "}";
    client.publish(bypassTopic.c_str(), bypassPayload.c_str(), true);

    // Repeat this pattern for relay and current...
    // Example: The E-Stop Binary Sensor
    // We add "value_template" so HA ignores the offset_ms and just looks at "val"
    String relayTopic = "homeassistant/binary_sensor/" + tool + "/relay/config";
    String relayPayload = "{"
        "\"name\":\"" + tool + " Relay Feedback\","
        "\"stat_t\":\"stat/" + tool + "/relay\","
        "\"val_tpl\":\"{{ value_json.val }}\"," 
        "\"dev_cla\":\"problem\""
    "}";
    client.publish(relayTopic.c_str(), relayPayload.c_str(), true);

    // Example: The E-Stop Binary Sensor
    // We add "value_template" so HA ignores the offset_ms and just looks at "val"
    String binCurTopic = "homeassistant/binary_sensor/" + tool + "/bincur/config";
    String binCurPayload = "{"
        "\"name\":\"" + tool + " Bin-Cur\","
        "\"stat_t\":\"stat/" + tool + "/bincur\","
        "\"val_tpl\":\"{{ value_json.val }}\"," 
        "\"dev_cla\":\"problem\""
    "}";
    client.publish(binCurTopic.c_str(), binCurPayload.c_str(), true);

    // New: A "Button" entity for the Reset Pulse
    String resetConfigTopic = "homeassistant/button/" + tool + "/reset/config";
    String resetPayload = "{"
        "\"name\":\"" + tool + " Force Reset\","
        "\"cmd_t\":\"cmnd/" + tool + "/reset\","
        "\"payload_press\":\"RESET\","
        "\"ic\":\"mdi:shredder\""
    "}";
    client.publish(resetConfigTopic.c_str(), resetPayload.c_str(), true);
}

bool mqttReconnect() {
    if (client.connect(TOOL_ID, MQTT_USER, MQTT_PASS)) {
        sendDiscovery(); // Announce self to HA
        client.subscribe(("cmnd/" + String(TOOL_ID) + "/#").c_str());
        return true;
    }
    return false;
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
 */
void monitorSignal(bool current, bool &last, unsigned long &dTimer, unsigned long &hTimer, const char* subTopic, bool useHeartbeat) {
    // 1. Handle State Changes (Debounced)
    if (current != last) {
        if (millis() - dTimer > 50) { 
            last = current;
            // MQTT logic: binary sensors usually want "ON" or "OFF" strings
            String valStr = current ? "ON" : "OFF";
            queueEvent(subTopic, valStr);
            
            if (useHeartbeat) hTimer = millis(); 
        }
    } else {
        dTimer = millis(); 
    }

    // 2. Handle "Still-Active" Heartbeats
    if (useHeartbeat && current && (millis() - hTimer > 30000)) {
        queueEvent(subTopic, "ON");
        hTimer = millis();
    }
}

//-----------------------------------------------------------------------------

#if HAS_NFC

// This is called inside processRfid() when a new card is scanned
void reportRfidScan(String uid) {
    // We send a JSON object so the server/HA gets the UID and the time-delta
    // The topic matches what we set in Discovery: stat/TOOL_ID/rfid
    queueEvent("rfid", "{\"uid\":\"" + uid + "\"}");
}

static String lastSeenUid = "";
static unsigned long lastScanAttempt = 0;
/*
Read RFID cards & Check authorization
>[!TIP] Generated with AI --> verify operation...
*/
void processRfid() {
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
        reportRfidScan(idString);
    }
}

#endif 