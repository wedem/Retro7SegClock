
char HOSTNAME[65] = "RetroClockESP8266";
char NTP_HOST[65] = "europe.pool.ntp.org";

int timeZoneOffset  = 60;                      // time offset to utc in minutes, 120 minutes -> CEST


#define WIFIMGR_PORTAL_TIMEOUT 180

uint8_t _ip[4] = {192,168,0,128};
uint8_t _gw[4] = {192,168,0,1};
uint8_t _sn[4] = {255,255,255,0};

bool updateConfig = false;  // For WiFiManger custom config and config
