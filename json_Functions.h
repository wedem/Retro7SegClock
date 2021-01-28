#include <ArduinoJson.h>         //https://github.com/bblanchon/ArduinoJson

#if defined(ARDUINOJSON_VERSION)
  #if !(ARDUINOJSON_VERSION_MAJOR == 6 and ARDUINOJSON_VERSION_MINOR >= 8)
    #error "Install ArduinoJson v6.8.x or higher"
  #endif
#endif


// ***************************************************************************
// Build WiFi Configuration in JSON 
// ***************************************************************************
char * listConfigJSON() {
    const size_t bufferSize = JSON_OBJECT_SIZE(10) + 150;
  DynamicJsonDocument jsonBuffer(bufferSize);
  JsonObject root = jsonBuffer.to<JsonObject>();
  root["hostname"] = HOSTNAME;
  root["ntp_host"]  = NTP_HOST;
  root["timeZone"]  = timeZoneOffset; 
  uint16_t msg_len = measureJson(root) + 1;

  char * buffer = (char *) malloc(msg_len);
  serializeJson(root, buffer, msg_len);
  jsonBuffer.clear();
  return buffer;
}

// ***************************************************************************
// Output WiFi Configuration JSON
// ***************************************************************************
void getConfigJSON() {
  char * buffer = listConfigJSON();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send ( 200, "application/json", buffer);
  free (buffer);
}

// ***************************************************************************
// Build Clock mode Configuration in JSON 
// ***************************************************************************
char * listStatusJSON() {
  const size_t bufferSize = JSON_OBJECT_SIZE(6) + 1500;  
  DynamicJsonDocument jsonBuffer(bufferSize);
  JsonObject root = jsonBuffer.to<JsonObject>();
  time_t    timestamp = now();
  char str_time[10];

  sprintf(str_time, "%02d:%02d:%02d", hour(timestamp),minute(timestamp),second(timestamp));
  root["time"] = str_time;
  root["palette"]  = paletteSelect;
  if(overlayMode==1)
    root["overlaymode"]  = "On";
  else
    root["overlaymode"]  = "Off"; 
  root["brightness"] = brightnessSelect;
  if(dotMode == 0)
    root["dots"]  = "Off";
  else
    root["dots"]  = "On";
  if(displayMode==0)
    root["displaymode"]  = "12h";
   else
    root["displaymode"]  = "24h"; 
   
  uint16_t msg_len = measureJson(root) + 1;
  char * buffer = (char *) malloc(msg_len);
  serializeJson(root, buffer, msg_len);
  jsonBuffer.clear();
  return buffer;
}

// ***************************************************************************
// Output Clock Configuration JSON
// ***************************************************************************
void getStatusJSON() {
  char * buffer = listStatusJSON();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send ( 200, "application/json", buffer);
  free (buffer);
}

// ***************************************************************************
// Build ESP8266 configuration JSON
// ***************************************************************************
char * listESPStateJSON() {
  const size_t bufferSize = JSON_OBJECT_SIZE(31) + 1500;
  DynamicJsonDocument jsonBuffer(bufferSize);
  JsonObject root = jsonBuffer.to<JsonObject>();
  root["HOSTNAME"] = HOSTNAME;
  root["version"] = 1;
  root["heap"] = ESP.getFreeHeap();
  root["sketch_size"] = ESP.getSketchSize();
  root["free_sketch_space"] = ESP.getFreeSketchSpace();
  root["flash_chip_size"] = ESP.getFlashChipSize();
  root["flash_chip_real_size"] = ESP.getFlashChipRealSize();
  root["flash_chip_speed"] = ESP.getFlashChipSpeed();
  root["sdk_version"] = ESP.getSdkVersion();
  root["core_version"] = ESP.getCoreVersion();
  root["cpu_freq"] = ESP.getCpuFreqMHz();
  root["chip_id"] = ESP.getFlashChipId();
  root["state_save"] = "SPIFFS";
  uint16_t msg_len = measureJson(root) + 1;
  char * buffer = (char *) malloc(msg_len);
  serializeJson(root, buffer, msg_len);
  jsonBuffer.clear();
  return buffer;
}
void getESPStateJSON() {
  char * buffer = listESPStateJSON();
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", buffer);
  free (buffer);
}
