# Preamp V1

This is the Arduino code to run a HiFi Preamplifier. It has the following features:

  * Volume control via encoder and Apple remote.
  * Input selection via Apple remote.
  * Start-up mute.
  * Volume fade when powered on.
  * Volume level memory (EEPROM).
  * NeoPixel shows volume level with colour.

Full details can be found here:
http://www.diyaudio.com/forums/analog-line-level/254244-building-complete-preamp-arduino-remote-volume-input-control.html

## Hardware

  * DAC8812: Controlled via SPI. This is the digital attenuator and controls the volume.
  * MCP23S08: Controlled via SPI. For the input selector and mute.
  * NeoPixel: Controlled via a single-wire control protocol.
  * IR sensor: Standard 38Khz module.

The IRremote library is from here:
https://github.com/z3t0/Arduino-IRremote

Adafruit NeoPixel library is from here:
https://github.com/adafruit/Adafruit_NeoPixel

## Photos

![GIF of preamp](http://giant.gfycat.com/SneakyBreakableHyrax.gif)

![Completed preamp 1](../blob/master/images/P8040008.JPG?raw=true)

![Completed preamp 2](../blob/master/images/P8040016.JPG?raw=true)

![Completed preamp 3](../blob/master/images/P8040071.JPG?raw=true)
