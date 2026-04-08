#include "hal.h"
#include "config_check.h"

/*
Initalize all GPIO pins.
*/
void initHardware() {
    // Basic Outputs always present
    pinMode(PIN_BUZZER, OUTPUT);
    pinMode(PIN_RED_LED, OUTPUT);
    pinMode(PIN_GREEN_LED, OUTPUT);
    pinMode(PIN_SET_RELAY, OUTPUT);
    pinMode(PIN_RESET_RELAY, OUTPUT);

    // Conditional E-Stops
    #if NUM_ESTOPS >= 1
      pinMode(PIN_ESTOP_1, INPUT_PULLUP);
    #endif
    #if NUM_ESTOPS == 2
      pinMode(PIN_ESTOP_2, INPUT_PULLUP);
    #endif

    // Monitor Pins
    pinMode(PIN_BIN_CUR, INPUT_PULLUP);
    pinMode(PIN_BYPASS_KEY, INPUT_PULLUP);
    pinMode(PIN_RELAY_FEEDBACK, INPUT_PULLUP);
}

/*
Observe E-Stop(s) as a unified entity.
RETURN: Boolean `true` if any E-Stop is active.
*/
bool getEStopActive() {
    #if NUM_ESTOPS == 2
        return (digitalRead(PIN_ESTOP_1) == LOW || digitalRead(PIN_ESTOP_2) == LOW);
    #elif NUM_ESTOPS == 1
        return (digitalRead(PIN_ESTOP_1) == LOW);
    #else
        return false; // No E-stops configured for this tool
    #endif
}

/*
Stage 2 relay - electronic self-latching status.
Measures closed/open of extra pole on 4PDT relay.
The proxy for "tool is currently in use" information. 
>[!TIP] could potentially need a debounce
*/
bool getRelayLatched(){
    return (digitalRead(PIN_RELAY_FEEDBACK) == LOW);
}

/*
The binary current (bin-cur) sensing input.
// commonly, an M3050 AC current switch
*/
bool getCurrentActive(){
    return (digitalRead(PIN_BIN_CUR) == LOW);
}

/*
Status of the physical bypass key.
This key is one of the inputs to 47HC02 control.
*/
bool getBypassActive(){
    return (digitalRead(PIN_BYPASS_KEY) == LOW);
}

/*
Currently unused.
*/
void setPreSetGate(bool open){
    //pass
    if(open){
        digitalWrite(PIN_SET_RELAY, HIGH);
    }else{
        digitalWrite(PIN_SET_RELAY, LOW);
    }
}

/*
RESET the stage 2 electronic self-latching relay.
> Commonly used to 'timeout' abandoned tools.
*/
void triggerResetPulse(){
    digitalWrite(PIN_RESET_RELAY, HIGH);
    delay(250); //want enough delay for S1 relay to operate 100% of the time
    digitalWrite(PIN_RESET_RELAY, LOW);
}

/*
Control two LEDs for RFID feedback to humans
*/
void setStatusLEDs(bool red, bool green){
    digitalWrite(PIN_RED_LED, red);
    digitalWrite(PIN_GREEN_LED, green);
}

/*
Currently unused.
*/
bool timelessTone(int dur){
    static unsigned long last_started = 0;
    static bool currently_playing = false;
    //does not use delay
    //turn the buzzer on or off
    if(!currently_playing){
        digitalWrite(PIN_BUZZER, HIGH);
        currently_playing = true;
        last_started = millis(); // vulnerable to rollover 
        return true;
    }
    if(millis()-last_started > dur){ // vulnerable to rollover
        digitalWrite(PIN_BUZZER, LOW);
        currently_playing = false;
        return false;
    }
}

/*
Currently unused.
*/
void playTone(int freq, int dur){
    //currently playing dumb
    timelessTone(dur);
    delay(dur+10);
    timelessTone(dur);
}