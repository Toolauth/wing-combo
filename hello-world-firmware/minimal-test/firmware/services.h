#include <WiFi.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config_check.h"

void initNetwork();
void handleOTA();
void enableTool();
void unEnableTool();
bool enable(bool relay, bool estop);
void sendHeartbeat();
bool handleAuthFlow(uint8_t * uid, uint8_t uidLength);
bool checkRemoteAuthorized(); 
void checkTimeout();
void sendEStopStatus(bool eStopState);
void sendBypassStatus(bool bypassState);
void sendRelayStatus(bool relayState);
void sendCurrentStatus(bool currentActive);
void sendErrorStatus(String err);
bool processRfid(); // Returns true ONLY if a valid, authorized card is scanned
void manageNetwork(); //recover from dropped WiFi