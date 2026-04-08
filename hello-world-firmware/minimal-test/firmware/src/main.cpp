#include "hal.h"
#include "services.h"
#include "config_check.h"

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
    manageNetwork();        // Background WiFi healing
    processNetworkQueue();  // Background data uploading

    static ToolState now;
    static ToolState last;
    
    // 1. Physical Sensing
    now.estopActive = getEStopActive();
    now.relayLatched = getRelayLatched();
    now.bypassActive = getBypassActive();
    now.currentDetected = getCurrentActive();

    // 2. Fire-and-Forget Reporting
    if(now.estopActive != last.estopActive){
        queueEvent("/status", "estop", String(now.estopActive));
        last.estopActive = now.estopActive;
    }
    if(now.relayLatched != last.relayLatched){
        queueEvent("/status", "relay", String(now.relayLatched)); 
        last.relayLatched = now.relayLatched;
    }
    if(now.bypassActive != last.bypassActive){
        queueEvent("/status", "bypass", String(now.bypassActive));
        last.bypassActive = now.bypassActive;
    }
    if(now.currentDetected != last.currentDetected){
        queueEvent("/status", "current", String(now.currentDetected));
        last.currentDetected = now.currentDetected;
    }
    static unsigned long currentHeartbeat = 0;
    if(now.currentDetected && millis() - currentHeartbeat > 30000){
        // more frequent heartbeat for tool-in-use & timeout
        queueEvent("/status", "current", String(now.currentDetected));
        currentHeartbeat = millis();
    }

    // 3. Network Heartbeat (Reports status to C&C)
    sendHeartbeat();

    // 4. RFID - empty, unless `HAS_NFC` is `true`
    processRfid();
    
    // 5. Remote Listener (Catches "Authorization" from server)
    // Server will respond "true" to /remote after it processes the /auth event
    if(!now.authorized){
        if(checkRemoteAuthorized()) now.authorized = true;
    }

    // 6. Refresh LEDs
    updateUI(now.estopActive, now.bypassActive, now.authorized);

    // ------------------------------------------------------------------------
    // 7. Start `Enable` (pre-SET) state
    // If tool is not blocked, or already enabled: provide access
    if(now.relayLatched){
        //tool just turned ON
        unEnableTool(); //--> so tool cannot be turned ON again
        now.authorized = false;
        last.authorized = false;
    }
    if(now.authorized && now.estopActive){
        //estop blocks activation
        queueEvent("/error", "err", "enable requested, tool estop active");
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

    // 8. End `Enable` (pre-SET) state
    // remote stop an inactive tool
    if(now.authorized && (millis() - last.enabled > ENABLE_DELAY)){
        //enable window has timed out
        unEnableTool();
        now.authorized = false;
        last.authorized = false;
    }

    // 9. Tool Timeout
    // remote stop an inactive tool
    checkTimeout();

    // 10. Short delay to stabilize things
    delay(10);
}