#include "hal.h"
#include "services.h"
#include "config_check.h"
#include <esp_task_wdt.h>

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
    
    // Watchdog for 8 seconds
    // Parameters: (timeout_seconds, panic_mode_enabled)
    esp_task_wdt_init(8, true); 
    esp_task_wdt_add(NULL); // Subscribes the main loop task to the watchdog

    initNetwork();

    #if HAS_NFC
        nfc.begin();
        nfc.SAMConfig();
    #endif
}

void loop() {
    // 0. Updates, Watchdog, WiFi, Queues
    ArduinoOTA.handle();    // Listen for firmware updates
    esp_task_wdt_reset();   // Resets the 8s timer each loop
    manageNetwork();        // Background WiFi healing
    processNetworkQueue();  // Background data uploading

    static ToolState now;
    static ToolState last;

    // Timer variables for the helper
    static unsigned long dEstop, hEstop;
    static unsigned long dBypass, hBypass;
    static unsigned long dRelay, hRelay;
    static unsigned long dCurrent, hCurrent;
    
    // 1. Physical Sensing
    now.estopActive = getEStopActive();
    now.relayLatched = getRelayLatched();
    now.bypassActive = getBypassActive();
    now.currentDetected = getCurrentActive();

    // 2. Fire-and-Forget Reporting
    monitorSignal(now.estopActive, last.estopActive, dEstop, hEstop, "estop");
    monitorSignal(now.bypassActive, last.bypassActive, dBypass, hBypass, "bypass");
    monitorSignal(now.relayLatched, last.relayLatched, dRelay, hRelay, "relay", true);
    monitorSignal(now.currentDetected, last.currentDetected, dCurrent, hCurrent, "current", true);
    
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