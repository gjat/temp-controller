# ESP_8266 Beer Brew Temperature Controller

An ESP8266 (AI-THINKER) module, connected to DS1621 I2C temperature sensor, driving a relay to turn a heater on and off, in order to monitor and regulate the temperature for beer being brewed inside of an old fridge.

The current, min and max temperatures are displayed on a text web page served by the device, along with a chart of half hour averages.    NTP is used to obtain a sense of current time, and to keep the clock in sync.

## TODO
- Bug fixes and polish

## Notes for programming / hardware

### Programmer board.
- It needs external power once the ESP8266 tries to connect to the WIFI.  Initial programming can be done with the USB/TTL supplied power, but it isn't enough to run the device when it tries to transmit to a WIFI network.
- Using an LD1117 3.3v regulator to supply external power.
- USB/TTL to programmer board - Rx to Tx, Tx to Rx.
- Boot into "programming mode" by holding down the reset and programming push buttons, releasing the reset button first, then the programming button.

TODO: Need a schematic / layout of both boards.

Roughly based on https://tttapa.github.io/ESP8266/Chap02%20-%20Hardware.html

Now using OTA updates for most changes, but initial programming will need to be via serial (USB/TTL).

### Running board
- Doesn't have reset / programming buttons.  Just adds the reset pullup.
- Exposes an I2C header for plugging in the temp sensor (3.3v power, ground included)
- Uses a IRF7401 MOSFET as the relay driver (because I had them lying around from fixing something else).  I used a 680 ohm resistor between GPIO 16 and the MOSFET's gate, for paranoid current protection for the ESP8266.   I also put a 10k resistor between the MOSFET's gate and ground (to in theory help turn off).  GPIO pins take some looking at on an ESP8266 board.  Get the right diagram.
- Regulates 12v to 5v using a switching regulator module (again, stuff I had lying around)
- Regulates the 5v to 3.3v using an LD1117 3.3v regulator.  The double regulator arrangement was because I wanted 12 volts for the relay, but didn't want to drop 12 - 3.3v = 8.7 volts over the LD1117, thinking it might get warm.
- Relay is between the 12v supply and the drain of the mosfet.  Mosfet's source is tied to ground.
- One day, I might add a second output to control "cooling" via the old fridge itself.  But for now, it's just heating.

I have noticed some "resets" due to the sensor board moving around.   I'm looking into adding some small resistors between the ESP8266 and the cable to the temperature sensor to avoid shorts, etc.

## References
https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/server-examples.html
https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html
