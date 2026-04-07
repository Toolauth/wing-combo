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
#endif