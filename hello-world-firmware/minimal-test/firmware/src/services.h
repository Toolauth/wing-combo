#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config_check.h"
#include <queue>

struct NetworkEvent {
    String path;
    String key;
    String value;
    unsigned long queuedAt; // The 'timestamp' when it happened
};

//networking
void initNetwork();
void manageNetwork(); //recover from dropped WiFi
//bool sendHttpData(String path, String key, String value, unsigend long offsetMs);
void processNetworkQueue(); // Call this in loop()
void queueEvent(String path, String key, String value);

//tool control
void enableTool();
void unEnableTool();
void updateUI(bool estop, bool bypass, bool authorized);
void monitorSignal(bool current, bool &last, unsigned long &dTimer, unsigned long &hTimer, const char* key, bool useHeartbeat = false);

// status update
void sendHeartbeat();
void checkTimeout();
bool checkRemoteAuthorized(); 

// sensing
void processRfid();