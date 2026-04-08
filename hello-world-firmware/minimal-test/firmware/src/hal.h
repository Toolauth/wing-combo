#include <Arduino.h>
#include "config_check.h"


// Conditional RFID reader
#if HAS_NFC
    #include <Adafruit_PN532.h>
    extern Adafruit_PN532 nfc;
#endif

#define PIN_BUZZER 16 //OUTPUT: feedback for the RFID card reads
#define PIN_RED_LED 5 //OUTPUT: feedback for RFID card reads
#define PIN_GREEN_LED 6 //OUTPUT: feedback for RFID card reads
#define PIN_SET_RELAY 18 //OUTPUT: enables the user to 'set' the electrically self latching relay
#define PIN_RESET_RELAY 17 //OUTPUT: resets the electrically self latching relay, commonly used for timout of an abandoned tool
#define PIN_ESTOP_1 13 //INPUT: senses the state of primary e-stop switch
#define PIN_ESTOP_2 12 //INPUT: senses the state of secondary e-stop switch
#define PIN_BIN_CUR 15  //INPUT: senses the state of a manually calibrated AC current switch (replaced the need for ADE7953 to monitor electric current)
#define PIN_BYPASS_KEY 8 //INPUT: senses the key of a 'manual bypass' key
#define PIN_RELAY_FEEDBACK 9 //INPUT: direct monitoring the state of the electrically self-latching secondary relay (using one of the isolated poles for a DC feedback

#define PIN_SDA 3 //I2C data line
#define PIN_SCL 4 //I2C clock line

void initHardware();
bool getEStopActive();
bool getRelayLatched();
bool getCurrentActive();
bool getBypassActive();
void setPreSetGate(bool open);
void triggerResetPulse();
void setStatusLEDs(bool red, bool green);