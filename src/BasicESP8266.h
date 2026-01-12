
/* ESP8266 Basic Programm
 * 
 * initializes SPIFFS and EEPROM
 * connects to WiFi if valid data in file config and AP flag not set 
 * otherwise switches to AP mode:
 * Ssid= "APxxxxxxxx"  (x=Chip mac)
 * Password = the last 8 characters of the ssid
 * AP address is 192.168.4.1
 * /mem?filename=xxx shows contents of file xxx
 * /mem?delete=xxx erases file xxx from SPIFFS
 * /info shows directory of SPIFFS, properties of the ESP8266 chip and provides a form to set WiFi and MQTT
 * 
 * resetting 5 times within 2 seconds sets the AP flag. The ESP will not switch to STA mode
 * and will not connect to Wifi specified in config
 * 
 * resetting 5 times within 2 seconds again switches off the AP mode
 * 
 * written by Dr. Hans-Jürgen Weber at 12.05.2020
 * last modification 10.04.2022
 * 
 */

#ifndef BasicESP8266_h
#define BasicESP8266_h

#include <ESP8266WiFi.h>
//#include <ESP8266mDNS.h>
#include "EEPROM.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoOTA.h>
#include "LittleFS.h"
#include <WiFiUdp.h>
#include <NTPClient.h>




#define DPR(args...)    Serial.print(args)
#define DPRLN(args...)    Serial.println(args)
#define DPRF(args...)    Serial.printf(args)
#define DPRW(args...) Serial.write(args)

#define ESIZE 16
#define MAXARGS 30
#define SHOWWIFIPWD false
#define ntp true
#define DEBUG_NTPClient 1


class BasicESP8266
{
  public:
    BasicESP8266(boolean debug, int sigLed, boolean sigLowActive, bool apPwd, bool showWifiPwd);
    void begin();
    void loop();
    bool setSig(uint32_t onDur, uint32_t offDur, uint8_t sigCount);
    bool sig(int n);
    String htmlMask(String a, bool citation, String title);
    String getPostParams(AsyncWebServerRequest *req);
    String getInitValue(String src, String key);
    int countInitKeys(String src);
    bool isInitKey(String src, String key);
    bool saveFile(String fname, String finhalt);
    String loadFile(String fname);
    String getSsid();
    String getPwd();
    String getIp();
    String getGateway();
    String getNetmask();
    void setSsid(String ssid);
    void setPwd(String pwd);
    void setIp(String ip);
    void setGateway(String gateway);
    void setNetmask(String netmask);
    void setUpdateInterval(String updateinterval);
    void setTZOffset(String tzoffset);
    void setupWifi(AsyncWebServerRequest *req);
    bool saveConfig();
    
    uint32_t realSize = ESP.getFlashChipRealSize();
    uint32_t ideSize = ESP.getFlashChipSize();
    uint32_t freeSpiffs=0;
    uint32_t totalSpiffs=0;
    uint32_t usedSpiffs=0;
    String chipMode;
    String sIP="";
    IPAddress localIPAdr={0,0,0,0};
    File f;    

    WiFiClient espClient;
    AsyncWebServer *server;
#ifdef ntp
    WiFiUDP ntpUDP;
    NTPClient *timeClient;
    uint32_t getEpochTime();

#endif

    unsigned long getUpdateInterval();
    bool apmode=true;
    bool mqtt=false;
    unsigned long connectedAt=0;

    String tempstr="";

    String apsnames[8]={"ssid","pwd","adr","gateway","mask","broker","topic","port"};
  
    String wifiHtml1=
    "<form action='/apsetup' method='POST'>\n";
    String wifiHtml2=
    "<table style='background-color:#ffff80;'>\n"
    "<tr><td>WiFi name (SSID):</td><td><input type='text' size='30' maxlength='80' name='ssid' id='ssid' value='##ssid'></td></tr>\n"
    "<tr><td>Password:</td><td><input type='text' size='30' maxlength='80' name='pwd' id='pwd' value='##pwd'></td></tr>\n"
    "<tr><td>Static IP address (empty=DHCP):</td><td><input type='text' size='15' maxlength='15' name='adr' id='adr' value='##ip'></td></tr>\n"
    "<tr><td>Gateway:</td><td><input type='text' size='15' maxlength='15' name='gateway' id='gateway' value='##gateway'></td></tr>\n"
    "<tr><td>Netmask:</td><td><input type='text' size='15' maxlength='15' name='mask' id='mask' value='##netmask'></td></tr>"
    "<tr><td>NTP Update Interval:</td><td><input type='text' size='15' maxlength='15' name='updateinterval' id='updateinterval' value='##updateinterval'></td></tr>"
    "<tr><td>Timezone offset (in seconds):</td><td><input type='text' size='15' maxlength='15' name='tzoffset' id='tzoffset' value='##tzoffset'></td></tr>"
    "<tr><td>&#160;</td><td>&#160;</td></tr>\n"
    "</table>\n";
    String wifiHtml3=
    "<br><input type='submit' value='ok' name='ok'>\n"
    "</form>\n";



   
  private:    void _setup(AsyncWebServerRequest *request);
    bool _apPwd;
    bool _showWifiPwd;
    String _getSpiffs(bool table);
    bool _deleteFile(String fname);
    uint32_t _eeGetULong(int adr);    // holt einen 32BitUWert von adr
    void _eePutULong(int adr, uint32_t val); // speichert einen 32BitUWert val an adr
    bool _setConfig();
    bool _tryWifi();
    void _checkResets();
    void _setserver();
    void _onChipInfo(AsyncWebServerRequest *request);
    void _index(AsyncWebServerRequest *request);
    int _setArgs(AsyncWebServerRequest *req);
    String _infStr(String dinfo);
    bool _inform();
    
    boolean _debug; 
    int _sigLed;
    boolean _sigLowActive;
    uint32_t _sigOn=0;
    uint32_t _sigOff=0;
    uint32_t _sigOnDur=0;
    uint32_t _sigOffDur=0;
    uint8_t _sigCount=0;
    
    uint32_t _resetCount=0;
    const uint32_t _RESETTIME=2000;  // Reset within 2 seconds
    const uint8_t _RESETLIMIT=5;     // 
    bool _apFlag=false;              // Accesspoint flag true, if no EEPROM information
    const uint8_t _IND_APFLAG=4;     // postition in EEPROM
    const uint8_t _IND_RESETCOUNT=0; 

    int _tries=0;
    const int _MaxTries=30;
    uint32_t _nextAPWifiCheck=0;
    const uint32_t _APWifiCheckIntervall=60000;           // check for Wifi every minute in AP-Mode
    
    String _eSsid="";
    String _ePwd="";
    IPAddress _eAdr;
    IPAddress _eGateway;
    IPAddress _eMask;
    unsigned long _updateinterval=1800000;
    int _tzoffset=-21600;
    String _argVal[MAXARGS];
    String _argKey[MAXARGS];
    int _argCount = 0;
    String _devTopic="";
    uint32_t _lastTry=0;
    uint32_t _ccTimer=0;
    uint32_t _connectionCheckTime=5000;
    uint8_t _n=0;    // for debugging
    unsigned long _pos=0;
    String _fn="";
    int _dumpFormat=0;     // 0= plain text,  1= html,  2=json

    String _mac="";
    
    String _chipInfo=
      "<table style='background-color:#d0d0d0';>\n"
      "<tr><td>Chip real size</td><td>##real</td><tr>\n"
      "<tr><td>Chip ide size</td><td>##ide</td><tr>\n"
      "<tr><td>Chip mode</td><td>##mode</td><tr>\n"
      "<tr><td>SPIFFS total memory</td><td>##total</td><tr>\n"
      "<tr><td>SPIFFS used memory</td><td>##used</td><tr>\n"
      "<tr><td>SPIFFS free memory</td><td>##free</td><tr>\n"
      "</table><br>\n";

};
#endif
