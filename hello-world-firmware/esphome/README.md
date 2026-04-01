# /esphome-hello-world
Here lives the simplest firmware examples, for use with ESPHome. 

They ought to connect all the standard/expected components, so that their state can be observed in [Home Assistant](https://www.home-assistant.io/) and [ESPHome](https://esphome.io/).

Here is the firmware that works.   
This was IRL tested with an [Adafruit Feather ESP32-S3 NoPSRAM](https://www.adafruit.com/product/5323) ([on platformio](https://docs.platformio.org/en/latest/boards/espressif32/adafruit_feather_esp32s3_nopsram.html))  

→ It appears the other [Feather Boards](https://learn.adafruit.com/adafruit-feather/feather-specification) have the same footprint, and all the “services” like `I2C` and `SPI` are in the same physical places. But the GPIO pin number can drift around from board to board: that is why this demo has GPIO you can commant/uncomment as needed. Also, to work in ESPHome, the board has to be supported by [Platformio](https://docs.platformio.org/en/latest/boards/)
