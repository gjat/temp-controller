# ESP_8266 Beer Brew Temperature Controller

An ESP8266 (AI-THINKER) module, connected to DS1621 I2C temperature sensor, driving a relay to turn a heater on and off, in order to monitor and regulate the temperature for beer being brewed inside of an old fridge.

The current, min and max temperatures are displayed on a text web page served by the device, along with hourly averages.    NTP is used to obtain a sense of current time, and to keep the clock in sync.

## TODO
- Fix loading and saving of temperatures (dealing with power cycles).  Somehow the max value isn't loading
- Nicer display (html)
- Configuration for the WIFI network to connect to, without having to hard-code a network password.  I assume the device would need to start up as an access point, and let you configure it, then reset into "normal" mode.
- Some form of OTA update would be nice.

## Notes for programming / hardware

### Programmer board.
- It needs external power once the ESP8266 tries to connect to the WIFI.  Initial programming can be done with the USB/TTL supplied power, but it isn't enough to run the device when it tries to transmit to a WIFI network.
- Using an LD1117 3.3v regulator to supply external power.
- USB/TTL to programmer board - Rx to Tx, Tx to Rx.
- Boot into "programming mode" by holding down the reset and programming push buttons, releasing the reset button first, then the programming button.

TODO: Need a schematic / layout of both boards.

### Running board
- Doesn't have reset / programming buttons
- Uses a IRF7401 MOSFET as the relay driver (because I had them lying around)
- Regulates 12v to 5v using a switching regulator module I had lying around (again)
- Regulates the 5v to 3.3v using an LD1117 3.3v regulator.
- One day, I might add a second output to control "cooling" via the old fridge itself.  But for now, it's just heating.

## References
https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/server-examples.html
https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html
