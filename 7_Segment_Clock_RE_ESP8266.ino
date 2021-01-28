// Retro 7 Segment Clock Firmware for ESP8266 based on the 
// Retro 7 Segment Clock - Regular Edition - Software update v6 / nodeMCU / ESP8266 support (experimental)
// https://www.thingiverse.com/thing:3014572
// 06/2020 - Daniel Cikic
// 01/2021 - Thomas Wedemeyer
// Serial is set to 74880

// Libraries used:
// Library FastLED in Version 3.4.0 
// Library Time-master in Version 1.6 
// Library EEPROM in Version 1.0 
// Library Ticker in Version 1.0 
// Library ESP8266WiFi in Version 1.0 
// Library DNSServer in Version 1.1.1 
// Library ESP8266WebServer in Version 1.0 
// Library WiFiManager in Version 0.16.0 
// Library ESP8266mDNS in Version 1.2 
// Library NTPClient in Version 3.2.0 
// Library ArduinoJson in Version 6.12.0 

#include "definition.h"

#define FASTLED_ESP8266_RAW_PIN_ORDER           // this means we'll be using the raw esp8266 pin order -> GPIO_12, which is d6 on nodeMCU
#define LED_PIN 12                              // led data in connected to GPIO_12 (d6/nodeMCU)
#define FASTLED_ALLOW_INTERRUPTS 0              // Disable Interrupts to prevent flicker on data transfer

// Definitions for the WS2812B LED Strip 
#define LED_PWR_LIMIT 1000                        // 750mA - Power limit in mA (voltage is set in setup() to 5v)
#define LED_DIGITS 6                             // 4 or 6 digits, can only be an even number as...
#define LED_PER_DIGITS_STRIP 47                  // ...two digits are made out of one piece of led strip with 47 leds...
#define LED_BETWEEN_DIGITS_STRIPS 5              // 5 leds between above strips - and all this gives us LED_COUNT... :D
#define LED_COUNT ( LED_DIGITS / 2 ) * LED_PER_DIGITS_STRIP + ( LED_DIGITS / 3 ) * LED_BETWEEN_DIGITS_STRIPS

#include <FastLED.h>                              // these libraries will be included in all cases....
#include <TimeLib.h>
#include <EEPROM.h>

// ***************************************************************************
// Load libraries for: WebServer / WiFiManager
// ***************************************************************************
#include <Ticker.h>
#include <ESP8266WiFi.h>           //https://github.com/esp8266/Arduino

// needed for library WiFiManager
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>           //https://github.com/tzapu/WiFiManager
#include <FS.h>

#include <ESP8266mDNS.h>

#include "data/index_htm_gz.h"
#include "htm_edit_gz.h"
#include "data/style_css.h"
#include "data/favicon_ico_gz.h"


// ***************************************************************************
// Instanciate HTTP(80) / WebSockets(81) Server
// ***************************************************************************
ESP8266WebServer server(80);

Ticker ticker;

void tick() {
  //toggle state
  uint16_t state = digitalRead(LED_BUILTIN);  // get the current state of GPIO1 pin
  digitalWrite(LED_BUILTIN, !state);     // set pin to the opposite state
}

// ***************************************************************************
// Saved state handling in WifiManager
// ***************************************************************************
// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, uint8_t index)
{
  uint8_t found = 0;
  uint8_t strIndex[] = {0, -1};
  uint8_t maxIndex = data.length()-1;

  for(uint8_t i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }
  String return_value = data.substring(strIndex[0], strIndex[1]);
  return_value.replace(" ", "");
  return found>index ? return_value : "";
}

// ***************************************************************************
// Callback for WiFiManager library when config mode is entered
// ***************************************************************************
//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  updateConfig = true;
}

// ***************************************************************************
// Include: Webserver
// ***************************************************************************
#include "spiffs_webserver.h"

// ***************************************************************************
// Include: helper functions
// ***************************************************************************
#include "helper_functions.h"




// ***************************************************************************
// Load libraries &  Setup for: NTP-Client 
// ***************************************************************************
#include <NTPClient.h>
#include <WiFiUdp.h>

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);   //


// ***************************************************************************
// Setup for the Clock
// ***************************************************************************
CRGB leds[LED_COUNT];

CRGBPalette16 currentPalette;

const bool dbg = true;                           // debug, true = enable serial input/output
int buttonA = 13;                               // momentary push button, 1 pin to gnd, 1 pin to d7 / GPIO_13
int buttonB = 14;                               // momentary push button, 1 pin to gnd, 1 pin to d5 / GPIO_14

byte brightness = 150;                           // default brightness if none saved to eeprom yet / first run
byte brightnessLevels[3] {100, 150, 230};        // 0 - 254, brightness Levels (min, med, max) - index (0-2) will get stored to eeprom
                                                 // Note: With brightnessAuto = 1 this will be the maximum brightness setting used!
byte brightnessAuto = 0;                         // 1 = enable brightness corrections using a photo resistor/readLDR();
byte upperLimitLDR = 140;                        // everything above this value will cause max brightness to be used (if it's higher than this)
byte lowerLimitLDR = 40;                         // everything below this value will cause minBrightness to be used
byte minBrightness = 20;                         // anything below this avgLDR value will be ignored
float factorLDR = 1.0;                           // try 0.5 - 2.0, compensation value for avgLDR. Set dbgLDR & dbg to true and watch serial console. Looking...
const bool dbgLDR = false;                       // ...for values in the range of 120-160 (medium room light), 40-80 (low light) and 0 - 20 in the dark
int pinLDR = 0;                                 // LDR connected to A0 (nodeMCU only offers this one)
byte intervalLDR = 100;                           // read value from LDR every 100ms (most LDRs have a minimum of about 30ms - 50ms)
unsigned long valueLDRLastRead = 0;              // time when we did the last readout
int avgLDR = 0;                                  // we will average this value somehow somewhere in readLDR();
int lastAvgLDR = 0;

byte startColor = 0;                             // "index" for the palette color used for drawing
byte displayMode = 0;                            // 0 = 12h, 1 = 24h (will be saved to EEPROM once set using buttons)
byte colorOffset = 32;                           // default distance between colors on the color palette used between digits/leds (in overlayMode)
int colorChangeInterval = 1500;                  // interval (ms) to change colors when not in overlayMode (per pixel/led coloring uses overlayInterval)
byte overlayMode = 0;                            // switch on/off (1/0) to use void colorOverlay(); (will be saved to EEPROM once set using buttons)
int overlayInterval = 200;                       // interval (ms) to change colors in overlayMode
byte paletteSelect = 2;                          // config parameter for the color palette (will be saved to EEPROM)
byte brightnessSelect = 3;                       // config parameter for the brightnesslevel; (will be saved to EEPROM)
byte dotMode= 0;                                 // config parameter for the blinking dots on/off ; (will be saved to EEPROM)
#define PALETTECOUNT    7                         // Nr of color palettes

byte btnRepeatCounter = 1;
byte lastKeyPressed = 0;
unsigned long btnRepeatStart = 0;

byte lastSecond = 0;
unsigned long lastLoop = 0;
unsigned long lastColorChange = 0;

void switchPalette();                             // Function prototypes
void switchBrightness();

// ***************************************************************************
// Include: other functions
// ***************************************************************************
#include "json_functions.h"
#include "filesystem_function.h"
#include "request_handlers.h"

// @todo config parameter / modes should be moved to the json 
/* these values will be stored to the EEPROM:
  0 = index for selectedPalette / switchPalette();
  1 = index for brightnessLevels / switchBrightness();
  2 = displayMode 12/24h
  3 = overlayMode on/off
  4 = dotMode (blinking dots on / off
*/

// ***************************************************************************
// LED/Segment configuration
// ***************************************************************************
byte segGroups[14][2] = {         // 14 segments per strip, each segment has 1-x led(s). So lets assign them in a way we get something similar for both digits
  // right (seen from front) digit. This is which led(s) can be seen in which of the 7 segments (two numbers: First and last led inside the segment, same on TE):
  {  6,  8 },                     // top, a
  {  9, 11 },                     // top right, b
  { 13, 15 },                     // bottom right, c
  { 16, 18 },                     // bottom, d
  { 19, 21 },                     // bottom left, e
  {  3,  5 },                     // top left, f
  {  0,  2 },                     // center, g
  // left (seen from front) digit
  { 38, 40 },                     // top, a
  { 41, 43 },                     // top right, b
  { 25, 27 },                     // bottom right, c
  { 28, 30 },                     // bottom, d
  { 31, 33 },                     // bottom left, e
  { 35, 37 },                     // top left, f
  { 44, 46 }                      // center, g
};
// Note: The first number always has to be the lower one as they're subtracted later on... (fix by using abs()? ^^)

// Using above arrays it's very easy to "talk" to the segments. Simply use 0-6 for the first 7 segments, add 7 (7-13) for the following ones per strip/two digits
byte digits[14][7] = {                    // Lets define 10 numbers (0-9) with 7 segments each, 1 = segment is on, 0 = segment is off
  {   1,   1,   1,   1,   1,   1,   0 },  // 0 -> Show segments a - f, don't show g (center one)
  {   0,   1,   1,   0,   0,   0,   0 },  // 1 -> Show segments b + c (top and bottom right), nothing else
  {   1,   1,   0,   1,   1,   0,   1 },  // 2 -> and so on...
  {   1,   1,   1,   1,   0,   0,   1 },  // 3
  {   0,   1,   1,   0,   0,   1,   1 },  // 4
  {   1,   0,   1,   1,   0,   1,   1 },  // 5
  {   1,   0,   1,   1,   1,   1,   1 },  // 6
  {   1,   1,   1,   0,   0,   0,   0 },  // 7
  {   1,   1,   1,   1,   1,   1,   1 },  // 8
  {   1,   1,   1,   1,   0,   1,   1 },  // 9
  {   0,   0,   0,   1,   1,   1,   1 },  // t -> some letters from here on (index 10-13, so this won't interfere with using digits 0-9 by using index 0-9
  {   0,   0,   0,   0,   1,   0,   1 },  // r
  {   0,   1,   1,   1,   0,   1,   1 },  // y
  {   0,   1,   1,   1,   1,   0,   1 }   // d
};

// ***************************************************************************
// Setup the System
// ***************************************************************************
void setup() {
  if (brightnessAuto == 1) pinMode(pinLDR, OUTPUT);
  pinMode(buttonA, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  if (dbg) {
    Serial.begin(74880);
    delay(500); 
    Serial.println();    
    Serial.println(F("Retro 7 Segment Clock v2 Regular Edition starting up..."));
    Serial.print(F("Configured for: ")); Serial.print(LED_COUNT); Serial.println(F(" leds"));
    Serial.print(F("Power limited to (mA): ")); Serial.print(LED_PWR_LIMIT); Serial.println(F(" mA")); Serial.println();
  }
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.5, tick);

  // ***************************************************************************
  // Setup: SPIFFS
  // ***************************************************************************
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      Serial.printf("FS File: %s, size: %s\r\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }

    FSInfo fs_info;
    SPIFFS.info(fs_info);
    Serial.printf("FS Usage: %d/%d bytes\r\n", fs_info.usedBytes, fs_info.totalBytes);
  }

  // ***************************************************************************
  // Setup: WiFiManager
  // ***************************************************************************
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  //Strip Config
  (readConfigFS()) ? Serial.println("WiFiManager config FS read success!"): Serial.println("WiFiManager config FS Read failure!");
  delay(250);
  WiFiManagerParameter custom_hostname("hostname", "Hostname", HOSTNAME, 64, " maxlength=64");
  
  char _ntp_offset[6]; //needed tempararily for WiFiManager Settings
  WiFiManagerParameter custom_ntp_host("host", "NTP hostname", NTP_HOST, 64, " maxlength=64");
  sprintf(_ntp_offset, "%d", timeZoneOffset);
  WiFiManagerParameter custom_ntp_timezoneOffset("offset", "Timeozone Offset", _ntp_offset, 5, " maxlength=5 type=\"number\"");

  //Local intialization. Once its business is done, there is no need to keep it around
  wifi_station_set_hostname(const_cast<char*>(HOSTNAME));
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  wifiManager.addParameter(&custom_hostname);
  wifiManager.addParameter(&custom_ntp_host);
  wifiManager.addParameter(&custom_ntp_timezoneOffset);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  // Uncomment if you want to restart ESP8266 if it cannot connect to WiFi.
  // Value in brackets is in seconds that WiFiManger waits until restart
#if defined(WIFIMGR_PORTAL_TIMEOUT)
  wifiManager.setConfigPortalTimeout(WIFIMGR_PORTAL_TIMEOUT);
#endif

 // Order is: IP, Gateway and Subnet
#if defined(WIFIMGR_SET_MANUAL_IP)
  wifiManager.setSTAStaticIPConfig(IPAddress(_ip[0], _ip[1], _ip[2], _ip[3]), IPAddress(_gw[0], _gw[1], _gw[2], _gw[3]), IPAddress(_sn[0], _sn[1], _sn[2], _sn[3]));
#endif

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(HOSTNAME)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();  //Will be removed when upgrading to standalone offline McLightingUI version
    delay(1000);  //Will be removed when upgrading to standalone offline McLightingUI version
  }

  //save the custom parameters to FS/EEPROM
  strcpy(HOSTNAME, custom_hostname.getValue());
  strcpy(NTP_HOST, custom_ntp_host.getValue());
  timeZoneOffset = atoi(custom_ntp_timezoneOffset.getValue());
  if (updateConfig) {
      (writeConfigFS(updateConfig)) ? Serial.println("WiFiManager config FS Save success!"): Serial.println("WiFiManager config FS Save failure!");
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();
  //keep LED on
  digitalWrite(LED_BUILTIN, LOW);



  // ***************************************************************************
  // Setup: MDNS responder
  // ***************************************************************************
  bool mdns_result = MDNS.begin(HOSTNAME);

  Serial.print("Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/ to open RetroClockESP8266.");

  Serial.print("Use http://");
  Serial.print(HOSTNAME);
  Serial.println(".local/ when you have Bonjour installed.");
  Serial.print("New users: Open http://");
  Serial.print(WiFi.localIP());
  Serial.println("/upload to upload the webpages first.");
  Serial.println("");

#include "rest_api.h"

  server.begin();

  // Start MDNS service
  if (mdns_result) {
    MDNS.addService("http", "tcp", 80);
  }

  // Init NTP client server host & timezone
  timeClient.setPoolServerName(NTP_HOST);
  timeClient.setTimeOffset(timeZoneOffset * 60);

  // Setup of the LEDs
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalSMD5050).setTemperature(DirectSunlight).setDither(1);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_PWR_LIMIT);
  FastLED.setMaxRefreshRate( 10 );
  FastLED.clear();
  FastLED.show();
  EEPROM.begin(512);
  loadValuesFromEEPROM();
  switchPalette();
  switchBrightness();

   if ( WiFi.status() == WL_CONNECTED ) {
      Serial.print(F("Connected to SSID: ")); Serial.println(WiFi.SSID());
      // if status is connected, initialize timeClient and sync to ntp
      timeClient.begin();
      syncNTP();
    }
    else
    {
      byte i = 30;
      while ( i > 0 ) {                                                                       // check for roughly 10 seconds (i = 30 * 333ms) and show a
        if ( WiFi.status() != WL_CONNECTED ) i--; else i = 0;                                 // countdown while waiting for WL_CONNECTED status
        showDigit(i / 3, startColor, 0);
        FastLED.show();
        FastLED.clear();
        delay(333);
      }
    }
}


// ***************************************************************************
// The MAIN LOOP:
// ***************************************************************************
void loop() {

  server.handleClient();
  
  if (  ( lastLoop - lastColorChange >= colorChangeInterval ) && ( overlayMode == 0 )         // if colorChangeInterval has been reached and overlayMode is disabled...
     || ( lastLoop - lastColorChange >= overlayInterval ) && ( overlayMode == 1 ) ) {         // ...or if overlayInterval has been reached and overlayMode is enabled...
    startColor++;                                                                             // increase startColor to "move" colors slowly across the digits/leds
    updateDisplay(startColor, colorOffset);
    lastColorChange = millis();
  }

  if ( lastSecond != second() ) {                                                             // if current second is different from last second drawn...
    updateDisplay(startColor, colorOffset);// lastSecond will be set in displayTime() and will be used for
    lastSecond = second();// redrawing regardless the digits count (HH:MM or HH:MM:SS)
  }

  // Important! Handling of external keys is still supported but not tested...
  if ( lastKeyPressed == 1 ) {                                                                // if buttonA is pressed...
    switchBrightnessByKey();                                                                       // ...switch to next brightness level
    updateDisplay(startColor, colorOffset);
    if ( btnRepeatCounter >= 20 ) {                                                           // if buttonA is held for a few seconds change overlayMode 0/1 (using colorOverlay())
      if ( overlayMode == 0 ) overlayMode = 1; else overlayMode = 0;
      updateDisplay(startColor, colorOffset);
      btnRepeatStart = millis();
    }
  }
  if ( lastKeyPressed == 2 ) {// if buttonB is pressed...
    switchPaletteByKey();       // ...switch between color palettes                                                                       
    updateDisplay(startColor, colorOffset);
    if ( btnRepeatCounter >= 20 ) {                                                           // if buttonB is held for a few seconds change displayMode 0/1 (12h/24h)...
      if ( displayMode == 0 ) displayMode = 1; else displayMode = 0;
      updateDisplay(startColor, colorOffset);
      //EEPROM.put(2, displayMode);                                                             // ...and write setting to eeprom
      //EEPROM.commit();
      btnRepeatStart = millis();
    }
  }
  if ( ( lastLoop - valueLDRLastRead >= intervalLDR ) && ( brightnessAuto == 1 ) ) {          // if LDR is enabled and sample interval has been reached...
    readLDR();                                                                                // ...call readLDR();
    if ( abs(avgLDR - lastAvgLDR) >= 5 ) {                                                    // only adjust current brightness if avgLDR has changed for more than +/- 5.
      updateDisplay(startColor, colorOffset);
      lastAvgLDR = avgLDR;
      if ( dbg ) { Serial.print(F("Updated display with avgLDR of: ")); Serial.println(avgLDR); }
    }
    valueLDRLastRead = millis();
  }

  if ( lastKeyPressed == 12 ) {                                                               // if buttonA + buttonB are pushed at the same time....
                                                                                              // ...and if using WiFi...
      initWPS();                                                                              // ...start WPS
  }
  
  if ( ( hour() == 3 || hour() == 9 || hour() == 15 || hour() == 21 ) &&                    // if hour is 3, 9, 15 or 21 and...
         ( minute() == 3 && second() == 0 ) ) {                                               // minute is 3 and second is 0....
      if ( dbg ) Serial.print(F("Current time: ")); Serial.println(now());
      syncNTP();                                                                            // ...either sync using ntp or...
      if ( dbg ) Serial.print(F("New time: ")); Serial.println(now());
    }
  ESP.wdtFeed();                                                                            // feed the watchdog each time loop() is cycled through, just in case...
  FastLED.show();                                                                             // run FastLED.show() every time to avoid color flickering at low brightness settings
  lastKeyPressed = readButtons();
  lastLoop = millis();
  if ( dbg ) dbgInput();                                                                      // if dbg = true this will read serial input/keys
}

// ***************************************************************************
// The Automatic brightness controll is not tested!
// ***************************************************************************
void readLDR() {                                                                                            // read LDR value 5 times and write average to avgLDR for use in updateDisplay();
  static byte runCounter = 1;
  static int tmp = 0;
  byte readOut = map(analogRead(pinLDR), 0, 1023, 0, 250);
  tmp += readOut;
  if (runCounter == 5) {
    avgLDR = (tmp / 5)  * factorLDR;
    tmp = 0; runCounter = 0;
    if ( dbg && dbgLDR ) { Serial.print(F("avgLDR value: ")); Serial.print(avgLDR); }
    avgLDR = max(avgLDR, int(minBrightness)); avgLDR = min(avgLDR, int(brightness));                        // this keeps avgLDR in a range between minBrightness and maximum current brightness
    if ( avgLDR >= upperLimitLDR && avgLDR < brightness ) avgLDR = brightness;                              // if avgLDR is above upperLimitLDR switch to max current brightness
    if ( avgLDR <= lowerLimitLDR ) avgLDR = minBrightness;                                                  // if avgLDR is below lowerLimitLDR switch to minBrightness
    if ( dbg && dbgLDR ) { Serial.print(F(" - adjusted to: ")); Serial.println(avgLDR); }
  }
  runCounter++;
}


// ***************************************************************************
// Color Overlay
// ***************************************************************************
void colorOverlay() {                                                                                       // This "projects" colors on already drawn leds before showing leds in updateDisplay();
  for (byte i = 0; i < LED_COUNT; i++) {                                                                    // check each led...
    if (leds[i])                                                                                            // ...and if it is lit...
      leds[i] = ColorFromPalette(currentPalette, startColor + (colorOffset * i), brightness, LINEARBLEND);  // ...assign increasing color from current palette
  }
}

// ***************************************************************************
// Update Display
// the actual setting of the Palette and brightness is automaticaly updated 
// in this function 
// ***************************************************************************
void updateDisplay(byte color, byte colorSpacing) {                                                         // this is what redraws the "screen"
  FastLED.clear();                                                                                          // clear whatever the leds might have assigned currently...
  switchBrightness();
  switchPalette();
  displayTime(now(), color, colorSpacing);                                                                  // ...set leds to display the time...
  if (overlayMode == 1) colorOverlay();                                                                     // ...and if using overlayMode = 1 draw custom colors over single leds
  if (brightnessAuto == 1) {                                                                                // If brightness is adjusted automatically by using readLDR()...
    FastLED.setBrightness(avgLDR);                                                                          // ...set brightness to avgLDR
  } else {                                                                                                  // ...otherwise...
    FastLED.setBrightness(brightness);                                                                      // ...assign currently selected brightness
  }
}

// ***************************************************************************
// ReadButton -> Not tested!
// ***************************************************************************
byte readButtons() {
  byte activeButton = 0;
  byte retVal = 0;
  static int btnRepeatDelay = 150;
  static unsigned long lastButtonPress = 0;
  if ( digitalRead(buttonA) == 0 || digitalRead(buttonB) == 0) {
    if (digitalRead(buttonA) == 0) activeButton = 1;
    else if (digitalRead(buttonB) == 0) activeButton = 2;
    if ( digitalRead(buttonA) == 0 && digitalRead(buttonB) == 0 ) activeButton = 12;
    if (millis() - lastButtonPress >= btnRepeatDelay) {
      btnRepeatStart = millis();
      btnRepeatCounter = 0;
      retVal = activeButton;
    } else if (millis() - btnRepeatStart >= btnRepeatDelay * (btnRepeatCounter + 1) ) {
      btnRepeatCounter++;
      if (btnRepeatCounter > 5) retVal = activeButton;
    }
    lastButtonPress = millis();
  }
  return retVal;
}

// ***************************************************************************
// Display the Time
// ***************************************************************************
void displayTime(time_t t, byte color, byte colorSpacing) {
  byte posOffset = 0;                                                                     // this offset will be used to move hours and minutes...
  if ( LED_DIGITS / 2 > 2 ) posOffset = 2;                                                // ... to the left so we have room for the seconds when there's 6 digits available
  if ( displayMode == 0 ) {                                                               // if 12h mode is selected...
    if ( hourFormat12(t) >= 10 ) showDigit(1, color + colorSpacing * 2, 3 + posOffset);   // ...and hour > 10, display 1 at position 3
    showDigit((hourFormat12(t) % 10), color + colorSpacing * 3, 2  + posOffset);          // display 2nd digit of HH anyways
  } else if ( displayMode == 1 ) {                                                        // if 24h mode is selected...
    if ( hour(t) > 9 ) showDigit(hour(t) / 10, color + colorSpacing * 2, 3 + posOffset);  // ...and hour > 9, show 1st digit at position 3 (this is to avoid a leading 0 from 0:00 - 9:00 in 24h mode)
    showDigit(hour(t) % 10, color + colorSpacing * 3, 2 + posOffset);                     // again, display 2nd digit of HH anyways
  }
  showDigit((minute(t) / 10), color + colorSpacing * 4, 1 + posOffset);                   // minutes thankfully don't differ between 12h/24h, so this can be outside the above if/else
  showDigit((minute(t) % 10), color + colorSpacing * 5, 0 + posOffset);                   // each digit is drawn with an increasing color (*2, *3, *4, *5) (*6 and *7 for seconds only in HH:MM:SS)
  if ( posOffset > 0 ) {
    showDigit((second(t) / 10), color + colorSpacing * 6, 1);
    showDigit((second(t) % 10), color + colorSpacing * 7, 0);
  }
  if(dotMode == 1)  // Blinking Mode on
  {
    if ( second(t) % 2 == 0 ) showDots(2, second(t) * 4.25);                                // show : between hours and minutes on even seconds with the color cycling through the palette once per minute
  }
  else // Blinking mode off
  {
    showDots(2, second(t) * 4.25);                                // show : between hours and minutes on even seconds with the color cycling through the palette once per minute
  }
  lastSecond = second(t);
}

void showSegment(byte segment, byte color, byte segDisplay) {
  // This shows the segments from top of the sketch on a given position (segDisplay).
  // pos 0 is the most right one (seen from the front) where data in is connected to the arduino
  byte leds_per_segment = 1 + abs( segGroups[segment][1] - segGroups[segment][0] );            // get difference between 2nd and 1st value in array to get led count for this segment
  if ( segDisplay % 2 != 0 ) segment += 7;                                                  // if segDisplay/position is odd add 7 to segment
  for (byte i = 0; i < leds_per_segment; i++) {                                             // fill all leds inside current segment with color
    leds[( segGroups[segment][0] + ( segDisplay / 2 ) * ( LED_PER_DIGITS_STRIP + LED_BETWEEN_DIGITS_STRIPS ) ) + i] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
  }
}

void showDigit(byte digit, byte color, byte pos) {
  // This draws numbers using the according segments as defined on top of the sketch (0 - 9)
  for (byte i = 0; i < 7; i++) {
    if (digits[digit][i] != 0) showSegment(i, color, pos);
  }
}

// ***************************************************************************
// ShowDots 
// ***************************************************************************
void showDots(byte dots, byte color) {
  // in 12h mode and while in setup upper dots resemble AM, all dots resemble PM
  byte startPos = LED_PER_DIGITS_STRIP;
  if ( LED_BETWEEN_DIGITS_STRIPS % 2 == 0 ) {                                                                 // only SE/TE should have a even amount here (0/2 leds between digits)
    leds[startPos] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
    if ( dots == 2 ) leds[startPos + 1] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
  } else {                                                                                                    // Regular and XL have 5 leds between digits
    leds[startPos] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
    leds[startPos + 1] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
    if ( LED_DIGITS / 3 > 1 ) {
        leds[startPos + LED_PER_DIGITS_STRIP + LED_BETWEEN_DIGITS_STRIPS] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
        leds[startPos + LED_PER_DIGITS_STRIP + LED_BETWEEN_DIGITS_STRIPS + 1] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
      }
    if ( dots == 2 ) {
      leds[startPos + 3] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
      leds[startPos + 4] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
      if ( LED_DIGITS / 3 > 1 ) {
        leds[startPos + LED_PER_DIGITS_STRIP + LED_BETWEEN_DIGITS_STRIPS + 3] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
        leds[startPos + LED_PER_DIGITS_STRIP + LED_BETWEEN_DIGITS_STRIPS + 4] = ColorFromPalette(currentPalette, color, brightness, LINEARBLEND);
      }
    }
  }
}

// ***************************************************************************
// Simple serial test interface 
// ***************************************************************************
void dbgInput() {
  if ( dbg ) {
    if ( Serial.available() > 0 ) {
      byte incomingByte = 0;
      incomingByte = Serial.read();
      if ( incomingByte == 44 ) // ,
      {
        if ( overlayMode == 0 )
        {
          overlayMode = 1; 
        }
        else overlayMode = 0;  
        EEPROM.put(3, overlayMode);    // ...and write setting to eeprom
        EEPROM.commit();               // on nodeMCU we need to commit the changes from ram to flash to make them permanent 
      }
      if ( incomingByte == 48 ) // 0
      { 
        if ( displayMode == 0 ) 
        {
          displayMode = 1; 
        }
        else displayMode = 0;  // 0
        EEPROM.put(2, displayMode);    // ...and write setting to eeprom
        EEPROM.commit();               // on nodeMCU we need to commit the changes from ram to flash to make them permanent         
      }
      if ( incomingByte == 49 ) FastLED.setTemperature(OvercastSky);                            // 1
      if ( incomingByte == 50 ) FastLED.setTemperature(DirectSunlight);                         // 2 
      if ( incomingByte == 51 ) FastLED.setTemperature(Halogen);                                // 3
      if ( incomingByte == 52 ) overlayInterval = 30;                                           // 4
      if ( incomingByte == 53 ) colorChangeInterval = 10;                                       // 5
      if ( incomingByte == 54 ) { overlayInterval = 200; colorChangeInterval = 1500; }          // 6
      if ( incomingByte == 55 ) lastKeyPressed = 1;                                             // 7
      if ( incomingByte == 56 ) lastKeyPressed = 2;                                             // 8
      //if ( incomingByte == 57 ) lastKeyPressed = 12;                                            // 9
      if ( incomingByte == 43 ) colorOffset += 16;                                              // +
      if ( incomingByte == 45 ) colorOffset -= 16;                                              // -
      Serial.print(F("Input - ASCII: ")); Serial.println(incomingByte, DEC); Serial.println();
    }
  }
}

// ***************************************************************************
// load the configuration parameters for the displaymodes etc. from the "eeprom"
// ***************************************************************************
void loadValuesFromEEPROM() {
  byte tmp = EEPROM.read(2);
  if ( tmp <= 1 ) displayMode = tmp; else displayMode = 0;        // if no values have been stored to eeprom at position 2/3 then a read will give us something unusable...
  tmp = EEPROM.read(3);                                           // ...so we'll only take the value from eeprom if it's smaller than 1
  if ( tmp <= 1 ) overlayMode = tmp; else overlayMode = 0;        // (for colorMode/displayMode which can only bei 0/1)
  tmp = EEPROM.read(4);
  if(tmp<=1) dotMode = tmp; 
    else  dotMode = 0;
  tmp = EEPROM.read(0);
  if(tmp<=6) paletteSelect = tmp; 
    else  paletteSelect = 0;
  tmp = EEPROM.read(1);
  if(tmp<=2) brightnessSelect = tmp; 
    else  brightnessSelect = 0;    
}

// ***************************************************************************
// SwithcPalette 
// ***************************************************************************
void switchPalette() {                                            // Simply add palettes, make sure paletteCount increases accordingly
  byte paletteCount = PALETTECOUNT;                                          // A few examples of gradients/solid colors by using RGB values or HTML Color Codes
  static byte selectedPalette = 0xFF;

  if(selectedPalette != paletteSelect)
  {
    if ( dbg ) { Serial.print(F("switchPalette()")); Serial.println(paletteSelect); }

    selectedPalette = paletteSelect;
  
    switch ( selectedPalette ) {
      case 0: currentPalette = CRGBPalette16( CRGB( 224,   0,  32 ),
                                              CRGB(   0,   0, 244 ),
                                              CRGB( 128,   0, 128 ),
                                              CRGB( 224,   0,  64 ) ); break;
      case 1: currentPalette = CRGBPalette16( CRGB( 224,  16,   0 ),
                                              CRGB( 192,  64,   0 ),
                                              CRGB( 128, 128,   0 ),
                                              CRGB( 224,  32,   0 ) ); break;
     case 2: currentPalette = CRGBPalette16( CRGB::Aquamarine,
                                              CRGB::Turquoise,
                                              CRGB::Blue,
                                              CRGB::DeepSkyBlue   ); break;
      case 3: currentPalette = RainbowColors_p; break;
      case 4: currentPalette = PartyColors_p; break;
      case 5: currentPalette = CRGBPalette16( CRGB::White ); break;
      case 6: currentPalette = CRGBPalette16( CRGB::LawnGreen ); break;
    }
  }
}

// ***************************************************************************
// Change Palette by external Key
// ***************************************************************************
void switchPaletteByKey()
{
    if (paletteSelect < PALETTECOUNT - 1) paletteSelect++; else paletteSelect = 0;
    EEPROM.put(0, paletteSelect);
    EEPROM.commit();
    if (dbg) { Serial.print(F("switchPalette() EEPROM write: ")); Serial.println(paletteSelect); Serial.println(); }
}


// ***************************************************************************
// Change Brightness
// ***************************************************************************
void switchBrightness() {                                  
  static byte selectedBrightness = 0xFF;

  if(selectedBrightness != brightnessSelect)
  {
    selectedBrightness = brightnessSelect;
  
    if ( dbg ) { Serial.print(F("switchBrightness() EEPROM value: ")); Serial.println(selectedBrightness); }
    switch ( selectedBrightness ) {
      case 0: brightness = brightnessLevels[selectedBrightness]; break;
      case 1: brightness = brightnessLevels[selectedBrightness]; break;
      case 2: brightness = brightnessLevels[selectedBrightness]; break;
    }
  }
}


// ***************************************************************************
// Change Palette by external Key
// ***************************************************************************
void switchBrightnessByKey() {
  if ( brightnessSelect < 2 ) brightnessSelect++; else brightnessSelect = 0;
  EEPROM.put(1, brightnessSelect);
  EEPROM.commit();
  if ( dbg ) { Serial.print(F("switchBrightness() EEPROM write: ")); Serial.println(brightnessSelect); Serial.println(); }
}


DEFINE_GRADIENT_PALETTE (setupColors_gp) {                                                  // this color palette will only be used while in setup
    1, 255,   0,   0,                                                                       // unset values = red, current value = yellow, set values = green
   63, 255, 255,   0,
  127,   0, 255,   0,
  191,   0, 255, 255,
  255,   0,   0, 255
};

/*
void setupClock() {
  // finally not using a custom displayTime routine for setup, improvising a bit and using the setupColor-Palette defined on top of the sketch
  if ( dbg ) Serial.println(F("Entering setup mode..."));
  byte prevBrightness = brightness;                                                         // store current brightness and switch back after setup
  brightness = brightnessLevels[1];                                                         // select medium brightness level
  currentPalette = setupColors_gp;                                                          // use setupColors_gp palette while in setup
  tmElements_t setupTime;                                                                   // Create a time element which will be used. Using the current time would
  setupTime.Hour = 12;                                                                      // give some problems (like time still running while setting hours/minutes)
  setupTime.Minute = 0;                                                                     // Setup starts at 12 (12 pm)
  setupTime.Second = 1;                                                                     // 1 because displayTime() will always display both dots at even seconds
  setupTime.Day = 20;                                                                       // not really neccessary as day/month aren't used but who cares ^^
  setupTime.Month = 9;                                                                      // see above
  setupTime.Year = 2019 - 1970;                                                             // yes... .Year is set by the difference since 1970. So "49" is what we want.
  byte setting = 1;                                                                         // counter to keep track of what's currently adjusted: 1 = hours, 2 = minutes
  byte blinkStep = 0;
  int blinkInterval = 500;
  unsigned long lastBlink = millis();
  FastLED.clear();
  FastLED.show();
  while ( digitalRead(buttonA) == 0 || digitalRead(buttonB) == 0 ) delay(20);               // this will keep the display blank while any of the keys is still pressed
  while ( setting <= LED_DIGITS / 2 ) {                                                     // 2 - only setup HH:MM. 3 - setup HH:MM:SS
    if ( lastKeyPressed == 1 ) setting += 1;                                                // one button will accept the current setting and proceed to the next one...
    if ( lastKeyPressed == 2 )                                                              // while the other button increases the current value
      if ( setting == 1 )                                                                   // if setting = 1 ...
        if ( setupTime.Hour < 23 ) setupTime.Hour += 1; else setupTime.Hour = 0;            // ...increase hour when buttonB is pressed
      else if ( setting == 2 )                                                              // else if setting = 2...
        if (setupTime.Minute < 59) setupTime.Minute += 1; else setupTime.Minute = 0;        // ...increase minute when buttonB is pressed
      else if ( setting == 3 )                                                              // else if setting = 3...
        if (setupTime.Second < 59) setupTime.Second += 1; else setupTime.Second = 0;        // ...increase second when buttonB is pressed
    if ( millis() - lastBlink >= blinkInterval ) {                                          // pretty sure there is a much easier and nicer way...
      if ( blinkStep == 0 ) { brightness = brightnessLevels[2]; blinkStep = 1; }            // ...to get the switch between min and max brightness (boolean?)
        else { brightness = brightnessLevels[0]; blinkStep = 0; }                           
      lastBlink = millis();
    }
    FastLED.clear();
    // Still a mess. But displayTime takes care of am/pm and positioning, so this draws the time
    // and redraws certain leds in different colors while in each setting
    // Maybe use the colorOffset in displayTime and manipulate/prepare palette only inside setup?
    if ( setting == 1 ) {
      displayTime(makeTime(setupTime), 63, 0);
      for ( byte i = 0; i < ( ( LED_DIGITS / 3 ) * LED_PER_DIGITS_STRIP + ( LED_DIGITS / 3 * LED_BETWEEN_DIGITS_STRIPS ) ); i++ ) 
        if ( leds[i] ) leds[i] = ColorFromPalette(currentPalette, 1, brightness, LINEARBLEND);
      if ( ( setupTime.Hour < 12 ) && ( displayMode == 0 ) ) showDots(1, 191); else showDots(2, 191);
    } else if ( setting == 2 ) {
      displayTime(makeTime(setupTime), 95, 0);
      for ( byte i = 0; i < ( ( LED_DIGITS / 3 ) * LED_PER_DIGITS_STRIP + ( LED_DIGITS / 3 * LED_BETWEEN_DIGITS_STRIPS ) ); i++ )
        if ( leds[i] ) leds[i] = ColorFromPalette(currentPalette, 63, brightness, LINEARBLEND);
      if ( LED_DIGITS / 3 > 1 ) for ( byte i = 0; i < LED_PER_DIGITS_STRIP; i++ )
        if ( leds[i] ) leds[i] = ColorFromPalette(currentPalette, 1, brightness, LINEARBLEND);
      if ( ( setupTime.Hour < 12 ) && ( displayMode == 0 ) ) showDots(1, 191); else showDots(2, 191);
    } else if ( setting == 3 ) {
      displayTime(makeTime(setupTime), 95, 0);
      for ( byte i = 0; i < LED_PER_DIGITS_STRIP; i++ )
        if ( leds[i] ) leds[i] = ColorFromPalette(currentPalette, 63, brightness, LINEARBLEND);
      if ( ( setupTime.Hour < 12 ) && ( displayMode == 0 ) ) showDots(1, 191); else showDots(2, 191);
    }
    FastLED.show();
    lastKeyPressed = readButtons();
    if ( dbg ) dbgInput();
  }
  Rtc.SetDateTime(makeTime(setupTime));                                                   // ...write setupTime to the rtc
  setTime(makeTime(setupTime));
  FastLED.clear(); displayTime(makeTime(setupTime), 95, 0); FastLED.show();
  brightness = prevBrightness;
  switchPalette();
  delay(500);                                                                                 // short delay followed by fading all leds to black
  for ( byte i = 0; i < 255; i++ ) {
    for ( byte x = 0; x < LED_COUNT; x++ ) leds[x]--;
    FastLED.show(); delay(2);
  }
  if ( dbg ) Serial.println(F("Setup done..."));
}*/



// ***************************************************************************
// TimeSync by NTP
// ***************************************************************************
void syncNTP() {                                                                            // gets time from ntp and sets internal time accordingly, will return when no connection is established
    if ( dbg ) Serial.println(F("Entering syncNTP()..."));
    if ( WiFi.status() != WL_CONNECTED ) {
      if ( dbg ) Serial.println(F("No active WiFi connection!"));
      return;
    }                                                                                         // Sometimes the connection doesn't work right away although status is WL_CONNECTED...
    delay(1500);                                                                              // ...so we'll wait a moment before causing network traffic
    timeClient.update();
    setTime(timeClient.getEpochTime());
    if ( dbg ) {
      Serial.print(F("nodemcu time: ")); Serial.println(now());
      Serial.print(F("ntp time    : ")); Serial.println(timeClient.getEpochTime());
      Serial.println(F("syncNTP() done..."));
    }
  }

  
// ***************************************************************************
// WPS Init -> NOT Tested with WiFiManagerSetup
// ***************************************************************************
void initWPS() {                                                                            // join network using wps. Will try for 5 times before exiting...
    byte i = 1;
    if ( dbg ) Serial.println(F("Initializing WPS setup..."));
    FastLED.clear();
    while ( i <= 5 ) {
      showDigit(10, 64, 3);
      showDigit(11, 64, 2);
      showDigit(12, 64, 1);
      showDigit(i, 0, 0);
      FastLED.show();
      FastLED.clear();
      if ( dbg ) Serial.println(F("Waiting for WiFi/WPS..."));
      ESP.wdtFeed();
      WiFi.beginWPSConfig();
      delay(500);
      if ( WiFi.SSID().length() <= 0 ) i++; else i = 6;
    }
    if ( WiFi.SSID().length() > 0 ) {                                                         // after getting the ssid we'll wait a few seconds and show a counter to make...
      showDigit(5, 0, 3);  showDigit(5, 0, 2);                                                // ...sure there's some time passing for dhcp/getting wifi settings
      showDigit(1, 0, 1);  showDigit(13, 0, 0);
      ESP.wdtFeed();
      FastLED.show(); delay(2000); FastLED.clear();
      if ( dbg ) {
        Serial.print(F("Connected to SSID: ")); Serial.println(WiFi.SSID());
        Serial.println(F("New WiFi connection, calling syncNTP..."));
      }
      for ( i = 5; i > 0; i--) {
        showDigit(i, 0, 0); FastLED.show(); delay(1000); FastLED.clear();
      }
      syncNTP();
    } else {
      if ( dbg ) { Serial.print(F("No connection established.")); Serial.println(WiFi.SSID()); }
    }
  }
