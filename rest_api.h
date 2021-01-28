#define USE_HTML_MIN_GZ

// ***************************************************************************
// Setup: Webserver handler
// ***************************************************************************
//list directory
server.on("/list", HTTP_GET, handleFileList);
//create file
server.on("/edit", HTTP_PUT, handleFileCreate);
//delete file
server.on("/edit", HTTP_DELETE, handleFileDelete);
//first callback is called after the request has ended with all parsed arguments
//second callback handles file uploads at that location
server.on("/edit", HTTP_POST, []() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "");
}, handleFileUpload);

// ***************************************************************************
// Setup: SPIFFS Webserver handler
// ***************************************************************************

server.on("/", HTTP_GET, [&](){
    server.sendHeader("Content-Encoding", "gzip", true);
    server.send_P(200, PSTR("text/html"), index_htm_gz, index_htm_gz_len);
});

server.on("/style.css", HTTP_GET, [&](){
    server.send_P(200, PSTR("text/css"), style_css, style_css_len);
});

server.on("/favicon.ico", HTTP_GET, [&](){
    server.sendHeader("Content-Encoding", "gzip", true);
    server.send_P(200, PSTR("text/plain"), favicon_ico_gz, favicon_ico_gz_len);
});


server.on("/edit", HTTP_GET, [&](){
  #if defined(USE_HTML_MIN_GZ)
    server.sendHeader("Content-Encoding", "gzip", true);
    server.send_P(200, PSTR("text/html"), edit_htm_gz, edit_htm_gz_len);
  #else
    if (!handleFileRead("/edit.htm"))
      handleNotFound();
  #endif
});


//called when the url is not defined here
//use it to load content from SPIFFS
server.onNotFound([]() {
  if (!handleFileRead(server.uri()))
    handleNotFound();
});

server.on("/upload", handleMinimalUpload);

server.on("/esp_status", HTTP_GET, []() { //get heap status, analog input value and all GPIO statuses in one json call 
  getESPStateJSON();
});

server.on("/restart", []() {
  Serial.printf("/restart\r\n");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "restarting..." );
  delay(1000);
  ESP.restart();
});

server.on("/reset_wlan", []() {
  Serial.printf("/reset_wlan\r\n");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Resetting WLAN and restarting..." );
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  ESP.restart();
});

server.on("/start_config_ap", []() {
  Serial.printf("/start_config_ap\r\n");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Starting config AP ..." );
  WiFiManager wifiManager;
  wifiManager.startConfigPortal(HOSTNAME);
});

server.on("/format_spiffs", []() {
  Serial.printf("/format_spiffs\r\n");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "text/plain", "Formatting SPIFFS ..." );
  SPIFFS.format();
});

server.on("/get_modes", []() {
  getConfigJSON(); //getModesJSON();
});

server.on("/status", HTTP_GET,[]() {
  getStatusJSON(); 
});


server.on("/setDisplayMode", []() {
  if ( displayMode == 0 ) displayMode = 1; else displayMode = 0;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(2, displayMode);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("DisplayMode = ");
  Serial.println(displayMode);
  server.send(200, "text/plain", 0 );

});


server.on("/setDots", []() {
  if ( dotMode == 0 ) dotMode = 1; else dotMode = 0;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(4, dotMode);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("dotMode = ");
  Serial.println(dotMode);
  server.send(200, "text/plain", 0 );
});

server.on("/setOverlayMode", []() {
  if ( overlayMode == 0 ) overlayMode = 1; else overlayMode = 0;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(3, overlayMode);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("overlayMode = ");
  Serial.println(overlayMode);
  server.send(200, "text/plain", 0 );
});


server.on("/setBrigthnessN", []() {
  if ( brightnessSelect > 0 ) 
    brightnessSelect--; 
  else brightnessSelect = 2;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(1, brightnessSelect);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("brightnessSelect = ");
  Serial.println(brightnessSelect);
  server.send(200, "text/plain", 0 );
});

server.on("/setBrigthnessP", []() {
  if ( brightnessSelect < 2 ) 
    brightnessSelect++; 
  else brightnessSelect = 0;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(1, brightnessSelect);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("brightnessSelect = ");
  Serial.println(brightnessSelect);
  server.send(200, "text/plain", 0 );
});

server.on("/setPaletteN", []() {
  if ( paletteSelect > 0 ) 
    paletteSelect--; 
  else paletteSelect = 6;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(0, paletteSelect);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("paletteSelect = ");
  Serial.println(paletteSelect);
  server.send(200, "text/plain", 0 );
});

server.on("/setPaletteP", []() {
  if ( paletteSelect < 6 ) 
    paletteSelect++; 
  else paletteSelect = 0;
  updateDisplay(startColor, colorOffset);
  EEPROM.put(0, paletteSelect);                                                             // ...and write setting to eeprom
  EEPROM.commit();

  Serial.print("paletteSelect = ");
  Serial.println(paletteSelect);
  server.send(200, "text/plain", 0 );
});
