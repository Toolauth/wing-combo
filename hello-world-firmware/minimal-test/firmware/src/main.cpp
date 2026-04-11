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
    // 1. Updates, Watchdog, WiFi, Queues
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
    
    // 2. Physical Sensing
    now.estopActive = getEStopActive();
    now.relayLatched = getRelayLatched();
    now.bypassActive = getBypassActive();
    now.currentDetected = getCurrentActive();

    // 3. Fire-and-Forget Reporting
    monitorSignal(now.estopActive, last.estopActive, dEstop, hEstop, "estop", false);
    monitorSignal(now.bypassActive, last.bypassActive, dBypass, hBypass, "bypass", false);
    monitorSignal(now.relayLatched, last.relayLatched, dRelay, hRelay, "relay", true);
    monitorSignal(now.currentDetected, last.currentDetected, dCurrent, hCurrent, "bincur", true);
    
    // 4. RFID - empty, unless `HAS_NFC` is `true`
    processRfid();

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

    // 10. Short delay to stabilize things
    delay(10);
}