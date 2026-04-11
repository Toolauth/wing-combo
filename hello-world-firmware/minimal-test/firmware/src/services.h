#include <WiFi.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h> // New dependency
#include <ArduinoJson.h>
#include "config_check.h"
#include <queue>

struct NetworkEvent {
    String topic;
    String value;
    unsigned long queuedAt;
};

// Networking & MQTT
void initNetwork();
void manageNetwork(); //recover from dropped WiFi
void processNetworkQueue(); // Call this in loop()
void queueEvent(String subPath, String value);

// MQTT Specifics
void mqttCallback(char* topic, byte* payload, unsigned int length);
void sendDiscovery();
bool mqttReconnect();

//tool control
void enableTool();
void unEnableTool();
void updateUI(bool estop, bool bypass, bool authorized);
void monitorSignal(bool current, bool &last, unsigned long &dTimer, unsigned long &hTimer, const char* subTopic, bool useHeartbeat);

// status update
void processRfid();