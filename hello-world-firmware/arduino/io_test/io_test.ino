// pin identifications
#define pin_sda 21
#define pin_scl 22
#define pin_irq 25 // not tested here
#define pin_red_led 32
#define pin_green_led 33
#define pin_reset_relay 17
#define pin_set_relay 16
#define pin_buzzer 5
#define pin_bypass_key 26
#define pin_estop_1 27
#define pin_estop_2 14


/* The block immediately below is for the ADE7953 example   *
 * https://github.com/CalPlug/ADE7953-Wattmeter/tree/master */
#define local_CLK pin_scl  //Set the CLK pin for I2C communication as pin 12
#define local_CS pin_sda   //Set the CS pin for I2C communication as pin 13
#define ADE7953_VERBOSE_DEBUG 1
#include <ADE7953_I2C.h>
#include <Wire.h>
ADE7953 myADE7953(local_CLK, local_CS); 

/* The block immediately below is for the rttl example. *
 * https://github.com/spicajames/Rtttl                  */
#define SPEAKER_PIN pin_buzzer 
#include <Rtttl.h>
Rtttl Rtttl(SPEAKER_PIN);
FLASH_STRING(song,"mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16c7,16p,16c7,16c7,p,16g6,16f#6,16f6,16d#6,16p,16e6,16p,16g#,16a,16c6,16p,16a,16c6,16d6,8p,16d#6,8p,16d6,8p,16c6");


void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("Hello World!");

    myADE7953.initialize();   //The ADE7953 must be initialized once in setup.

    Serial.println("testing Rttl/Buzzer...");
    Rtttl.play(song);

    pinMode(pin_red_led, OUTPUT);
    pinMode(pin_green_led, OUTPUT);
    pinMode(pin_reset_relay, OUTPUT);
    pinMode(pin_set_relay, OUTPUT);

    pinMode(pin_bypass_key, INPUT);
    pinMode(pin_estop_1, INPUT);
    pinMode(pin_estop_2, INPUT);
}

void loop() {
    // ADE7953 test
    long apnoload, activeEnergyA;
    float vRMS, iRMSA, powerFactorA, apparentPowerA, reactivePowerA, activePowerA;
    Serial.println("\n\n\n Testing ADE7953 after 1.0sec.....");
    delay(1000);
    Serial.print("In the IrmsA(A)\n");
    iRMSA = myADE7953.getIrmsA();  
    Serial.print("IrmsA (mA): ");
    Serial.println(iRMSA);
    delay(200);

    // Rtttl test
    Rtttl.updateMelody();

    // LED test
    Serial.println("\n\n\n Testing Red & Green LEDs after 1.0sec.....");
    delay(1000);
    digitalWrite(pin_red_led, HIGH);
    delay(500);
    digitalWrite(pin_red_led, LOW);
    digitalWrite(pin_green_led, HIGH);
    delay(500);
    digitalWrite(pin_green_led, LOW);

    // Relay test
    Serial.println("\n\n\n Testing Set & Reset Relays after 1.0sec.....");
    delay(1000);
    Serial.println("Set Relay ON");
    digitalWrite(pin_set_relay, HIGH);
    delay(500);
    Serial.println("Set Relay OFF");
    digitalWrite(pin_set_relay, LOW);
    delay(500);
    Serial.println("Reset Relay ON");
    digitalWrite(pin_reset_relay, HIGH);
    delay(500);
    Serial.println("Reset Relay OFF");
    digitalWrite(pin_reset_relay, LOW);

    // Input tests
    Serial.println("\n\n\n Testing the three inputs for the next 10 seconds.....");
    unsigned long start_time = millis();
    while (millis() - start_time < 10000) {
        Serial.print("Bypass Key: ");
        Serial.print(digitalRead(pin_bypass_key));
        Serial.print("\t Estop 1: ");
        Serial.print(digitalRead(pin_estop_1));
        Serial.print("\t Estop 2: ");
        Serial.print(digitalRead(pin_estop_2));
        Serial.println();
        delay(100);
    }

    Serial.println("\n\n\n Restarting the full testing cycle.");
    Serial.println("-------------------------------------------------");
}
