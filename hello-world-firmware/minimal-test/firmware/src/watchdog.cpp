#include "watchdog.h"
#include "services.h"
#include "hal.h"
#include <WiFi.h>
#include <PubSubClient.h>

RTC_DATA_ATTR int bootCount;
/*
Increment boot counter, determine reason.
Send both back to Server at the time of boot.
ref: https://docs.espressif.com/projects/arduino-esp32/en/latest/api/reset_reason.html#example
*/
void rebootCounter(){
    bootCount++;
    String reason = "";

    switch (rtc_get_reset_reason(0)) {
        case 1:  reason = "Vbat power on reset"; break;
        case 3:  reason = "Software reset digital core"; break;
        case 4:  reason = "Legacy watch dog reset digital core"; break;
        case 5:  reason = "Deep Sleep reset digital core"; break;
        case 6:  reason = "Reset by SLC module, reset digital core"; break;
        case 7:  reason = "Timer Group0 Watch dog reset digital core"; break;
        case 8:  reason = "Timer Group1 Watch dog reset digital core"; break;
        case 9:  reason = "RTC Watch dog Reset digital core"; break;
        case 10: reason = "Instrusion tested to reset CPU"; break;
        case 11: reason = "Time Group reset CPU"; break;
        case 12: reason = "Software reset CPU"; break;
        case 13: reason = "RTC Watch dog Reset CPU"; break;
        case 14: reason = "for APP CPU, reset by PRO CPU"; break;
        case 15: reason = "Reset when the vdd voltage is not stable"; break;
        case 16: reason = "RTC Watch dog reset digital core and rtc module"; break;
        default: reason = "NO_MEAN";
    }

    queueEvent("sw_reboot_count", String(bootCount));
    queueEvent("sw_reboot_reason", reason);
    
}

unsigned long lastSuccessfulCommunication;

/*
Check if we've been ghosted for too long.
Software reset if the device can't reach the server.
*/
void selfSoftwareRestart(){
    // Use the global timestamp from services.cpp
    extern unsigned long lastSuccessfulCommunication;
    extern PubSubClient client;


    bool pingSucceeds = client.connected();
    if (pingSucceeds) lastSuccessfulCommunication = millis();

    if (millis() - lastSuccessfulCommunication > MAX_OFFLINE_MS) {

        // so the tool doesn't shut off, as a user-surprise
        digitalWrite(PIN_RESET_RELAY, LOW); 

        // Give RESET_RELAY time to move
        delay(100); 
        ESP.restart(); 
    }
}