#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config_check.h"

void initNetwork();
void enableTool();
void unEnableTool();
void sendHeartbeat();
//bool sendHttpData(String path, String key, String value);
bool checkRemoteAuthorized(); 
void checkTimeout();
void sendEStopStatus(bool eStopState);
void sendBypassStatus(bool bypassState);
void sendRelayStatus(bool relayState);
void sendCurrentStatus(bool currentActive);
void sendErrorStatus(String err);
bool processRfid(); // Returns true ONLY if a valid, authorized card is scanned
void manageNetwork(); //recover from dropped WiFi
void updateUI(bool estop, bool bypass, bool authorized);