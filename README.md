# Combined Use Board
This is the **flagship board** for the [Toolauth](https://github.com/Toolauth) project.  More information about use, setup, and firmware is available in [this repo's wiki](https://github.com/Toolauth/wing-combo/wiki).

![Combined use circuit board, front and back](/docs/img/offset-rt-both.png)

It can allow for the management of power tools, and serve as the gateway for user authorization in a shared workshop.

The board has the following features:
* A stack of relays & a contactor for controlling power to the downstream tool.
* Its own power supply, to draw from the line supplying the downstream tool (_no extra power needed, if under 220VAC_).
* An ADE7953 current sensor, to monitor energy use/ status of the connected tool.
* Connectors to add in an RFID card reader (_workshop member's badges can swipe_).
* Connections for a hardware bypass, for quick-enabling of the tool.
* Simple connection for panel-mounted Red & Green status LEDs.
* 5V supply (from onboard transformer) in USB and solder-able form, for future expansion.

## How do I play with this PCB design?
It is as easy as downloading this design by whatever means you like: zip file, `clone`, etc. Then, open the `wing-combo.kicad_pro` file with any version of [KiCAD](https://www.kicad.org/) you like, but the newest version will probably be best.

## Adafuit Feather Specification
In avoidance of obsolescence, we have designed this board to the [Adafruit](https://www.adafruit.com/)'s [Feather Specification](https://learn.adafruit.com/adafruit-feather/feather-specification). The logic here is that the Feather specification itself should allow us to upgrade the 'brain' of this wing without needing a major re-design. 

_Yes, we could have designed a specific ESP32 into the board. But experience with previous generations has taught us that it is more convenient to make the boards with a MAC address removable._

## IOT Hosting
This project was designed to work with [Home Assistant](https://www.home-assistant.io/) and [ESPHome](https://esphome.io/), because they allow us to host the whole solution on-premises. However, this board is not tied to that stack by any means. There are lots of ways firmware and software could come together to make this work.

## Design Software
* Schematic and board designed in [KiCAD](https://www.kicad.org/).
* Graphics designed in [Inkscape](https://inkscape.org/).
* Ordered as assembled boards from [JLCPCB](https://jlcpcb.com/).