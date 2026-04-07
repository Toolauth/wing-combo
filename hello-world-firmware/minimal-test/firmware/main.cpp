#include "hal.h"
#include "services.h"
#include "config_check.h"

#if HAS_NFC
    #include <Adafruit_PN532.h>
    Adafruit_PN532 nfc(PIN_SDA, PIN_SCL);
#endif

void setup() {
    initHardware();
    initNetwork();

    #if HAS_NFC
        nfc.begin();
        nfc.SAMConfig();
    #endif
}

void loop() {
    // 0. hardware updates 
    static bool lastEStopActive = false;
    bool estop = getEStopActive();
    if(estop != lastEStopActive){
        sendEStopStatus(estop);
        lastEStopActive = estop;
    }
    static bool lastRelayLatched = false;
    bool relay = getRelayLatched();
    if(relay != lastRelayLatched){
        sendRelayStatus(relay);
        lastRelayLatched = relay;
    }
    static bool lastBypassActive = false;
    bool bypass = getBypassActive();
    if(bypass != lastBypassActive){
        sendBypassStatus(bypass);
        lastBypassActive = bypass;
    }

    static bool lastCurrentActive = false;
    bool current = getCurrentActive();
    if(current != lastCurrentActive){
        sendCurrentStatus(current);
        lastCurrentActive = current;
    }
    static unsigned long currentHeartbeat = 0;
    if(current && millis() - currentHeartbeat > 30000){
        // more frequent heartbeat for tool-in-use
        sendCurrentStatus(current);
        currentHeartbeat = millis();
    }

    // 1. Network Heartbeat (Reports status to C&C)
    sendHeartbeat();

    static bool currentlyEnabled = false;
    // 2. RFID - Only compiles if HAS_NFC is 1
    #if HAS_NFC
        uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
        uint8_t uidLength;
        if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 50)) {
            currentlyEnabled = handleAuthFlow(uid, uidLength);
        }
    #endif

    // 3. Remote Listeners
    // Check if the central server sent an "Unlock" command via WebSocket or API
    if(checkRemoteAuthorized()) currentlyEnabled = true;

    // ------------------------------------------------------------------------
    // 4. Handle Enabled state
    // If tool is not blocked, or already enabled: provide access
    if(relay){
        //tool just turned ON
        unEnableTool(); //so tool cannot be turned ON again
        currentlyEnabled = false;
    }
    if(currentlyEnabled && estop){
        //estop blocks activation
        sendErrorStatus("enable requested, tool estop active");
        currentlyEnabled = false;
    }
    static unsigned long lastEnableStart = 0;
    if(currentlyEnabled && millis() - lastEnableStart > ENABLE_DELAY){
        //enabled window shas timed out
        unEnableTool();
        currentlyEnabled = false;
    }
    if (currentlyEnabled){
        //must actually be ready to start
        enableTool();
    }
    // ------------------------------------------------------------------------

    // 5. Tool Timeout
    // remote stop an inactive tool
    checkTimeout();

    delay(10);
}