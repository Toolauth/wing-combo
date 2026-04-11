#ifndef CONFIG_CHECK_H
  #define CONFIG_CHECK_H

  // Default values if not specified in platformio.ini
  #ifndef NUM_ESTOPS
    #define NUM_ESTOPS 1
  #endif

  #ifndef HAS_NFC
    #define HAS_NFC 1
  #endif

  #ifndef ENABLE_DELAY
    #define ENABLE_DELAY 1000*60*2
  #endif

  #ifndef SCAN_COOLDOWN
    #define SCAN_COOLDOWN 2500 // 2.5 seconds before the same card can trigger again
  #endif

  #ifndef MAX_OFFLINE_MS
    #define MAX_OFFLINE_MS 300000 //5 minutes
  #endif

// ----------------------------------------------
// for Home Assistant Compliance
  #ifndef MQTT_SERVER
    #define MQTT_SERVER "homeassistant.local"
  #endif

  #ifndef MQTT_USER
    #define MQTT_USER "mqtt_user"
  #endif

  #ifndef MQTT_PASS
    #define MQTT_PASS "mqtt_password"
  #endif

  #ifndef MQTT_PORT
    #define MQTT_PORT 1883
  #endif
#endif