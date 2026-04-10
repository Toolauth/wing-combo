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
#endif