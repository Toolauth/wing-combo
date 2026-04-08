#include "hal.h"
#include "services.h"
#include "config_check.h"

#if HAS_NFC
    #include <Adafruit_PN532.h>
    Adafruit_PN532 nfc(PIN_SDA, PIN_SCL);
#endif

struct ToolState {
    bool bypassActive;
    bool estopActive;
    bool authorized;    // Set by Remote Auth
    unsigned long enabled; // timer (only use last)
    bool relayLatched;
    bool currentDetected;
};

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
    static ToolState now;
    static ToolState last;
    
    now.estopActive = getEStopActive();
    if(now.estopActive != last.estopActive){
        sendEStopStatus(now.estopActive);
        last.estopActive = now.estopActive;
    }
    now.relayLatched = getRelayLatched();
    if(now.relayLatched != last.relayLatched){
        sendRelayStatus(now.relayLatched);
        last.relayLatched = now.relayLatched;
    }
    now.bypassActive = getBypassActive();
    if(now.bypassActive != last.bypassActive){
        if(now.bypassActive){
            setStatusLEDs(LOW,HIGH);
        }else{
            setStatusLEDs(LOW,LOW);
        }
        sendBypassStatus(now.bypassActive);
        last.bypassActive = now.bypassActive;
    }
    
    now.currentDetected = getCurrentActive();
    if(now.currentDetected != last.currentDetected){
        sendCurrentStatus(now.currentDetected);
        last.currentDetected = now.currentDetected;
    }
    static unsigned long currentHeartbeat = 0;
    if(now.currentDetected && millis() - currentHeartbeat > 30000){
        // more frequent heartbeat for tool-in-use & timeout
        sendCurrentStatus(now.currentDetected);
        currentHeartbeat = millis();
    }

    // 2. Network Heartbeat (Reports status to C&C)
    sendHeartbeat();

    // 3. RFID - Only compiles if HAS_NFC is 1
    // If ANY card in the wallet is authorized, this returns true
    #if HAS_NFC
    processRfid();
    #endif
    
    // 4. Remote Listeners
    // Check if the central server sent an "Unlock" command via WebSocket or API
    if(checkRemoteAuthorized()) now.authorized = true;

    // ------------------------------------------------------------------------
    // 5. Start `Enable` (pre-SET) state
    // If tool is not blocked, or already enabled: provide access
    if(now.relayLatched){
        //tool just turned ON
        unEnableTool(); //--> so tool cannot be turned ON again
        now.authorized = false;
        last.authorized = false;
    }
    if(now.authorized && now.estopActive){
        //estop blocks activation
        sendErrorStatus("enable requested, tool estop active");
        unEnableTool(); //--> so tool cannot be turned ON again
        now.authorized = false;
        last.authorized = false;
    }
    if (now.authorized && !last.authorized){
        //must actually be ready to start
        enableTool();
        last.enabled = millis();
        last.authorized = true;
    }

    // ------------------------------------------------------------------------

    // 6. End `Enable` (pre-SET) state
    // remote stop an inactive tool
    if(now.authorized && (millis() - last.enabled > ENABLE_DELAY)){
        //enable window has timed out
        unEnableTool();
        now.authorized = false;
        last.authorized = false;
    }

    // 6. Tool Timeout
    // remote stop an inactive tool
    checkTimeout();

    // 7. Refresh LEDs
    updateUI(now.estopActive, now.bypassActive, now.authorized);

    delay(10);
}