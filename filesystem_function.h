 
// ***************************************************************************
// Reaad/Write of the json Configuratuion File
// ***************************************************************************
 /*
  Ticker save_state;
  Ticker save_seg_state;
  */
  Ticker save_conf;

  void tickerSaveConfig(){
    updateConfig = true;
  }
 
  // Write configuration to FS JSON
  bool writeConfigFS(bool save){
    if (save) {
      //FS save
      Serial.println("Saving config: ");    
      File configFile = SPIFFS.open("/config.json", "w");
      if (!configFile) {
        Serial.println("Failed!");
        save_conf.detach();
        updateConfig = false;
        return false;
      }
      Serial.println(listConfigJSON());
      configFile.print(listConfigJSON());
      configFile.close();
      save_conf.detach();
      updateConfig = false;
      return true;
      //end save
    } else {
      Serial.println("SaveConfig is false!");
      return false;
    }
  }
 
  // Read search_str to FS
  bool readConfigFS() {
    //read configuration from FS JSON
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.print("Reading config file... ");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("Opened!");
        size_t size = configFile.size();
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        configFile.close();
        const size_t bufferSize = JSON_OBJECT_SIZE(11) + 150;     /// <<< hier muss man mal gucken...
        DynamicJsonDocument jsonBuffer(bufferSize);
        DeserializationError error = deserializeJson(jsonBuffer, buf.get());
        Serial.print("Config: ");
        if (!error) {
          Serial.println("Parsed!");
          JsonObject root = jsonBuffer.as<JsonObject>();
          serializeJson(root, Serial);
          Serial.println("");
          strcpy(HOSTNAME, root["hostname"]);
          strcpy(NTP_HOST, root["ntp_host"]);
          timeZoneOffset = root["timeZone"].as<uint16_t>();
          jsonBuffer.clear();
          return true;
        } else {
          Serial.print("Failed to load json config: ");
          Serial.println(error.c_str());
          jsonBuffer.clear();
        }
      } else {
        Serial.println("Failed to open /config.json");
      }
    } else {
      Serial.println("Coudnt find config.json");
      writeConfigFS(true);
    }
    //end read
    return false;
  }
