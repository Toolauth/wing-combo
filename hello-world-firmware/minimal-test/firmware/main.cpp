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
    // 0. wifi health check
    manageNetwork();

    // 1. hardware updates 
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
        if(bypass){
            setStatusLEDs(LOW,HIGH);
        }else{
            setStatusLEDs(LOW,LOW);
        }
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
        // more frequent heartbeat for tool-in-use & timeout
        sendCurrentStatus(current);
        currentHeartbeat = millis();
    }

    // 2. Network Heartbeat (Reports status to C&C)
    sendHeartbeat();

    static bool startEnable = false;
    // 3. RFID - Only compiles if HAS_NFC is 1
    // If ANY card in the wallet is authorized, this returns true
    #if HAS_NFC
        if(processRfid()){
            startEnable = true;
        }
    #endif

    // 4. Remote Listeners
    // Check if the central server sent an "Unlock" command via WebSocket or API
    if(!startEnable && checkRemoteAuthorized()){
        startEnable = true;
    }

    // ------------------------------------------------------------------------
    // 5. Start `Enable` (pre-SET) state
    // If tool is not blocked, or already enabled: provide access
    if(relay){
        //tool just turned ON
        unEnableTool(); //--> so tool cannot be turned ON again
        startEnable = false;
    }
    if(startEnable && estop){
        //estop blocks activation
        sendErrorStatus("enable requested, tool estop active");
        unEnableTool(); //--> so tool cannot be turned ON again
        startEnable = false;
    }
    static unsigned long lastEnableStart = 0;
    if (startEnable){
        //must actually be ready to start
        enableTool();
        lastEnableStart = millis();
        startEnable = false;
    }

    // ------------------------------------------------------------------------

    // 6. End `Enable` (pre-SET) state
    // remote stop an inactive tool
    if(millis() - lastEnableStart > ENABLE_DELAY){
        //enable window has timed out
        unEnableTool();
    }

    // 6. Tool Timeout
    // remote stop an inactive tool
    checkTimeout();

    // 7. Refresh LEDs if in bypass mode
    if(bypass) setStatusLEDs(LOW,HIGH);

    delay(10);
}