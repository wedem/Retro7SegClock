# Retro7SegClock
Retro 7 Segment Clock Firmware for ESP8266

based on the 
Retro 7 Segment Clock - Regular Edition - Software update v6 / nodeMCU / ESP8266 support (experimental)
https://www.thingiverse.com/thing:3014572 from Daniel Cikic - 06/2020

This an alternative firmware for the 3D printed "Retro 7 Segment Clock" from Daniel Cikic for the ESP8266 NODEMCU-Module.
With this firmware the clock can be controlled by an ESP8266-module and the time is synchronized by an NTP-Client over a WiFi connection.

Changes to the original firmware:
  * Added WiFi Manager for WiFi-Setup
  * Added Web-Interface for configuration of the clock display modes
  * Fixed flicker problem controlling the WS2812 leds
  * Added extra display mode blinking dots on/off
  * Added support for automatic summer/winter time
  * Added support for timezones
  
Features from the original firmware still supported but not tested:
  * WiFi WPS setup
  * Setup of the display mode via external buttons
  * automatic brightness control


License: CC BY-NC 4.0
