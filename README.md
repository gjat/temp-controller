# ESP_8266 Beer Brew Temperature Controller

This is a personal project that I made to help me brew beer.   It could easily be used for any other simple temperature monitoring and control purposes. It uses An ESP8266 (AI-THINKER) module to monitor and chart temperature through an accessible web page.
The software can also control a heater to prevent the brew getting too cold. The temperature sensor itself is a DS1621 I2C chip (8 pin DIP), which measures temperature in 0.5°C steps.

The temperatures are charted as half hourly averages, and then current, minimum and maximum values are also displayed.  This is all visible via a web page served by the device over WiFi. The NTP protocol is used to obtain current time, and to keep the clock in sync.

To deal with resets and reprogramming, the configuration and the last 24 hour chart values are stored in flash memory.  So nothing is lost by unplugging and moving equipment around.

![screenshot](./pics/brewmonitor%20screenshot.jpg)

## Notes for programming / hardware

### Programmer board.
- I initially tried to program the module using only power from the USB to TTL adapter.  But, the module needs more power (external supply) once it tries to connect to the WIFI.
- I've used a LD1117 3.3v regulator to supply external power, because that's what I had in my parts box.  It seems to work well.
- When connecting the USB/TTL to programmer board, a "cross over" connection is needed.  i.e. Rx to Tx, Tx to Rx.
- To boot into "programming mode", hold down the reset and programming push buttons, then release the reset button first.  After this, you can take your hand away, and the device will be ready to accept new code from the Arduino IDE.

The programmer board is roughly based on https://tttapa.github.io/ESP8266/Chap02%20-%20Hardware.html

Now that I'm using OTA updates for most changes, the programming board isn't getting much use.   But it's necessary for initial programming via the serial port (USB/TTL).

### Running board
- To save parts, and time soldering, the "temperature controller" doesn't have the reset / programming buttons.  It just has the reset pullup resistor so the module will start on power up.
- The board exposes an I2C header for plugging in the temp sensor (3.3v power, ground included).  Some isolation / current limiting resistors between the sensor and the ESP module would be a worthwhile improvement, as a short in the sensor's cable would impact the ESP module.  For now, careful with my wiring, and willing to replace the cheap modules if I did fry one.
- The relay is driven via a IRF7401 MOSFET (again, because I had them lying around from fixing something else).  I used a 680 ohm resistor between GPIO 16 and the MOSFET's gate.   I also put a 10k resistor between the MOSFET's gate and ground (to help the MOSFET turn off).  GPIO pins take some looking at on an ESP8266 board.  Get the right diagram for your module.
- I've used two regulators for power. This is because the initial power supply is 12 volts to drive the relay, and it's a big drop to regulate from 12 volts to 3.3 volts in a linear regulator.  12 - 3.3 = 8.7 volts.  Times the 170 mA current potentially required, is 1.5 watts.  So the first regulator takes 12v to 5v, which I've used a switching regulator module (something I had lying around).  Then the 5v is regulated down to 3.3v using an LD1117 3.3v.  Choosing a 5 volt relay would simplify this, and the whole setup ran from an old USB power adapter.
- The relay's coil is between the 12v supply and the drain of the mosfet.  The Mosfet's source is tied to ground.
- Another improvement, would be to add a second output to control "cooling", by turning on the old fridge itself.  But for now, most of my brewing is in colder temperatures and cooling isn't an issue.

I have noticed some "resets" due to the sensor board moving around, or the power supply not keeping up with the current draw of the relay energising and the ESP module.  I'll look into adding another power filtering capacitor, or using a more modern relay.

## References
https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/server-examples.html

https://tttapa.github.io/ESP8266/Chap10%20-%20Simple%20Web%20Server.html
