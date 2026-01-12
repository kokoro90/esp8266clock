#include "BasicESP8266.h"

BasicESP8266::BasicESP8266(boolean debug, int sigLed, boolean sigLowActive, bool apPwd=false, bool showWifiPwd=false)
{
  _apPwd=apPwd;
  _showWifiPwd=showWifiPwd;
  _debug=debug;
  _sigLed=sigLed;
  _sigLowActive=sigLowActive;
  if (_debug) Serial.begin(115200);
}

void BasicESP8266::begin()
{
  delay(1000);
  if (_debug) Serial.println("\nsigLed: "+String(_sigLed)); 
  pinMode(_sigLed,OUTPUT);
//  if (_sigLed==1) pinMode(_sigLed,FUNCTION_3+OUTPUT);
  digitalWrite(_sigLed,_sigLowActive?HIGH:LOW);

//---------------------------------------------- EEPROM ----------------------------------------------------------------------------------------
  EEPROM.begin(ESIZE);            // EEPROM einschalten
  if (_debug) DPRF("EEPROM-size: %u\n",EEPROM.length());
  _apFlag=_eeGetULong(_IND_APFLAG)==1?true:false;
  if (_debug) DPRF ("AP flag: %u\n",_apFlag);
  _checkResets();


  FlashMode_t ideMode = ESP.getFlashChipMode();
  String chipMode=  ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT" : ideMode == FM_DIO ? "DIO" : ideMode == FM_DOUT ? "DOUT" : "UNKNOWN";

//---------------------------------------------- SPIFFS ----------------------------------------------------------------------------------------

  if (LittleFS.begin())
  {
    if (_debug)DPRLN ("\nSPIFFS Initialization....OK");
    FSInfo inf;
    LittleFS.info(inf);
    freeSpiffs=inf.totalBytes-inf.usedBytes;
    totalSpiffs=inf.totalBytes;
    usedSpiffs=inf.usedBytes;
    if (_debug)
    {
      DPRLN ("Total SPIFFS memory: "+String(inf.totalBytes));
      DPRLN ("Used SPIFFS memory: "+String(inf.usedBytes));
      DPRLN ("Free SPIFFS memory: "+String(freeSpiffs));
      DPRLN ("SPIFFS Dir:");
      DPRLN (_getSpiffs(false));
    }
  }
  else
  {
    if (_debug) DPRLN ("\nSPIFFS Initialisierung...Fehler!\nTry to format ...");
    boolean fsf=LittleFS.format();
    if (_debug) DPRLN(fsf?"\nFormatted. Reset device!\n":"\nCould not format device ...\n");
  }
  
  if (_debug) 
  {
    DPRF("\nChip real size: %u\n",realSize);
    DPRF("Chip ide size: %u\n",ideSize);
    DPRF("Chip write mode: %s\n",chipMode.c_str());
  }

  _setConfig();
  _tryWifi();

#ifdef ntp
  timeClient=new NTPClient(ntpUDP, "pool.ntp.org", _tzoffset, _updateinterval);
  timeClient->begin();
  timeClient->update();
#endif

  if (!apmode)
  {
    ArduinoOTA.setHostname(_mac.c_str());
    ArduinoOTA.onStart([this]() {String type=ArduinoOTA.getCommand() == U_FLASH? "sketch":"filesystem";if (this->_debug) Serial.println("Start OTA updating " + type);});
    ArduinoOTA.onEnd([this]() {if (this->_debug)Serial.println("\nEnd");ESP.restart();});
    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {if (this->_debug) Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    ArduinoOTA.onError([this](ota_error_t error) {if (this->_debug) Serial.printf("Error[%u]: ", error);});});
    ArduinoOTA.begin();
    if (_debug) Serial.println("OTA Ready");
  }

}

#ifdef ntp
uint32_t BasicESP8266::getEpochTime()
{
  timeClient->update();
  return timeClient->getEpochTime();
}
#endif

unsigned long BasicESP8266::getUpdateInterval()
{
  return _updateinterval;
}

//-----------------------------------------------  SigLed functions ------------------------------------------------------------------------

bool BasicESP8266::setSig(uint32_t onDur, uint32_t offDur, uint8_t sigCount)
{ 
  if (_sigOn>0 || _sigOff>0) return false;
  _sigOnDur=onDur;
  _sigOffDur=offDur;
  _sigCount=sigCount;
  _sigOn=millis();
  _sigCount--;
  return true;
}

bool BasicESP8266::sig(int n=3)
{
  return setSig(300,500,n);
}


//-----------------------------------------------  Spiff functions ------------------------------------------------------------------------

String BasicESP8266::_getSpiffs(bool table=false)
{
  String fn = "";
  String res="";
  if (table) res = "<table><tr style=\"background-color:lime;\"><th>Filename</th><th>Size (Bytes)</th></tr>\n";
  Dir dir = LittleFS.openDir("/");
  while (dir.next())
  {
    fn = dir.fileName();
    if (fn.substring(0, 1) == "/") fn = fn.substring(1);
    if (table)res += "<tr style=\"background-color:yellow;\"><td>"+fn + "</td><td>" + dir.fileSize() + "</td></tr>\n";
    else res+=fn+" ("+dir.fileSize()+")\n";
  }
  if (table) res+="<tr style=\"background-color:lightBlue;\"><td>Free memory:</td><td>"+String(freeSpiffs)+"</table>\n";
  return res;
}


bool BasicESP8266::saveFile(String fname, String finhalt)
{
  if (fname.substring(0, 1) != "/") fname = "/" + fname;
  File f = LittleFS.open(fname, "w+");
  if (!f)
  {
    if (_debug) DPRLN ("File Open Error!");
    return false;
  }
  else
  {
    if (!f.print(finhalt))
    {
      if (_debug) DPRLN ("Write Error!");
      return false;
    }
    f.close();
  }
  return true;
}

String BasicESP8266::loadFile(String fname)
{
  String res = "-1";
  File f = LittleFS.open("/" + fname, "r");
  if (f != false)
  {
    res = "";
    while (f.available()) res += (char)f.read();
    f.close();
  }
  else if (_debug) DPRLN(fname + " can't be loaded!");
  return res;
}

bool BasicESP8266::_deleteFile(String fname)
{
  return (LittleFS.remove("/"+fname)==true);
}

//-----------------------------------------------  EEPROM functions ------------------------------------------------------------------------

uint32_t BasicESP8266::_eeGetULong(int adr)    // holt einen 32BitUWert von adr
{
  uint32_t v = 0;
  for (int i = 0; i < 4; i++) v = (v << 8) + EEPROM.read(adr + i);
  return v;
}

void BasicESP8266::_eePutULong(int adr, uint32_t val) // speichert einen 32BitUWert val an adr
{
  for (int i = 3; i >= 0; i--)
  {
    EEPROM.write(adr + i, val & 0xff);
    val = (val >> 8);
  }
  EEPROM.commit();
}

//-----------------------------------------------  general functions ------------------------------------------------------------------------
 
String BasicESP8266::getSsid() {return _eSsid;}
String BasicESP8266::getPwd() {return _ePwd;}
String BasicESP8266::getIp() {return _eAdr[0]>0?_eAdr.toString():"";}
String BasicESP8266::getGateway() {return _eGateway[0]>0?_eGateway.toString():"";}
String BasicESP8266::getNetmask() {return _eMask[0]>0?_eMask.toString():"255.255.255.0";}
void BasicESP8266::setSsid(String ssid) {_eSsid=ssid;}
void BasicESP8266::setPwd(String pwd) {_ePwd=pwd;}
void BasicESP8266::setIp(String ip) {_eAdr.fromString(ip);}
void BasicESP8266::setGateway(String gateway) {_eGateway.fromString(gateway);}
void BasicESP8266::setNetmask(String netmask) {_eMask.fromString(netmask);}
void BasicESP8266::setUpdateInterval(String updateinterval) {_updateinterval=updateinterval.toInt();}
void BasicESP8266::setTZOffset(String tzoffset) {_tzoffset=tzoffset.toInt();}
String BasicESP8266::htmlMask(String a, bool citation=true, String title="")
{
  if (citation)
  {
    a.replace("&", "&amp;");
    a.replace("<", "&lt;");
    a.replace(">", "&gt;");
    a.replace("\"", "&quot;");
  }
  String res="<!DOCTYPE html>\n<html>\n<head>\n<meta charset='UTF-8'>\n<title>\n"+title+"\n</title>\n</head>\n<body style=\"font-family:arial,sans-serif,helvetica;\">\n";
  res+=citation?"<pre>\n":"";
  res+=a;
  res+=citation?"</pre>\n":"";
  res+="</body>\n</html>\n";
  return res;
}

int BasicESP8266::countInitKeys(String src)
{
  int n=0;
  for(int i=0;i<src.length();i++) if (src.charAt(i)=='=') n++;
  return n;
}

bool BasicESP8266::isInitKey(String src, String key)
{
   return src.indexOf(key+"=")==-1?false:true;
}

String BasicESP8266::getInitValue(String src, String key)
{
  int p=src.indexOf(key+"=");
  if (p==-1) return "";
  int q=src.indexOf("=",p);
  if (q==-1) return "";
  int r=src.indexOf("\n",p);
  if (r==-1) r=src.length();
  src= src.substring(q+1,r);
  src.trim();
  return src;
}

bool BasicESP8266::saveConfig()
{
  String msg="";
  msg+="ssid="+_eSsid+"\n";
  msg+="pwd="+_ePwd+"\n";
  msg+="adr="+getIp()+"\n";
  msg+="gateway="+getGateway()+"\n";
  msg+="mask="+getNetmask()+"\n";

//  if (_debug) DPRLN (msg);
  return saveFile("config",msg);
}

void BasicESP8266::setupWifi(AsyncWebServerRequest *req)
{
  _setup(req);
  saveConfig();
}

bool BasicESP8266::_setConfig()
{
  if (_debug) DPRLN("loading file 'config'");
  String fconfig=loadFile("config");
  if (fconfig=="-1") return false;
  if (_debug) DPRLN(fconfig);
  _eSsid=getInitValue(fconfig,"ssid");
  _ePwd=getInitValue(fconfig,"pwd");
  _eAdr.fromString(getInitValue(fconfig,"adr"));
  _eGateway.fromString(getInitValue(fconfig,"gateway"));
  _eMask.fromString(getInitValue(fconfig,"mask"));
  return true;
}

bool BasicESP8266::_tryWifi()
{
  _nextAPWifiCheck=millis()+_APWifiCheckIntervall;
  if (_eSsid!="" && _ePwd!="" && !_apFlag)
  {
    WiFi.mode(WIFI_STA);
    if (_eAdr[0]>0) WiFi.config(_eAdr, _eGateway, _eGateway, _eMask);
    WiFi.begin(_eSsid, _ePwd);
    _tries=0;
    if (_debug) DPRLN("\nTrying to connect\n");
    while(WiFi.status() != WL_CONNECTED && _tries< _MaxTries)
    {
       _tries++;
       if (_debug) DPR(".");
       delay(500);
    }
    if (_debug) DPRLN("\nConnected\n");
    if (WiFi.status()== WL_CONNECTED)
    {
      if (_debug) DPRF("\nConnected to %s\nIP-Address: %s\nHostname: %s\n",_eSsid.c_str(),WiFi.localIP().toString().c_str(),WiFi.hostname().c_str());
      sIP=WiFi.localIP().toString();
      localIPAdr=WiFi.localIP();
      apmode=false;
      connectedAt=millis();
      _apFlag=0;
      _eePutULong(_IND_APFLAG, (long)_apFlag);
      _tries=0;
      _setserver();
      sig(3);
      return true;
    }
  }

    if (_debug) DPRLN("\nConfiguring access point");
    WiFi.mode(WIFI_AP);
    _mac="ESP"+WiFi.macAddress();
    _mac.replace(":","");
    if (_debug) DPRLN ("mac: "+_mac);
    if (WiFi.softAP(_mac,_apPwd?_mac.substring(_mac.length()-8,_mac.length()):""))   // if _withPwd sets password for ap-mode (last 8 digits of mac)
    {
      apmode=true;
      sIP=WiFi.softAPIP().toString();
      if (_debug) DPRLN("\nAP + server " + _mac + " at http://" + WiFi.softAPIP().toString() + " started");
      _nextAPWifiCheck=millis()+_APWifiCheckIntervall;
    }
    else if (_debug) DPRLN("\nno AP possible");

  _setserver();
  return false;
}

//-----------------------------------------------------------------------------------------------------------------

void BasicESP8266::_checkResets()
{
  _resetCount = _eeGetULong(_IND_RESETCOUNT);
  if (_debug) DPRF ("Resets within 2 s: %u\n",_resetCount);
  _resetCount++;
  _eePutULong(_IND_RESETCOUNT, (long)_resetCount);
  if (_resetCount >= _RESETLIMIT-1)
  {
    _apFlag=!_apFlag;
    _eePutULong(_IND_RESETCOUNT, (long)0);
    _eePutULong(_IND_APFLAG, (long)_apFlag);
  }
}

//-----------------------------------------------  Server configuration ------------------------------------------------------------------------
  int BasicESP8266::_setArgs(AsyncWebServerRequest *req)
  {
    _argCount = 0;
    for (int i=0; i<req->args() && i<MAXARGS; i++)
    {
      _argKey[i] = req->argName(i);
      _argVal[i] = req->arg(i);
      _argCount++;
    }
    return _argCount;
  }

String BasicESP8266::getPostParams(AsyncWebServerRequest *request)
{
  int count=_setArgs(request);
  String msg="";
  for (int i=0;i<count;i++) msg+=_argKey[i]+"="+_argVal[i]+"\n";
  if (_debug) DPRLN (msg);
  return msg;
}

void  BasicESP8266::_setup(AsyncWebServerRequest *request)
  {
  
   String ht=wifiHtml1+wifiHtml2+wifiHtml3;
    ht.replace("##ssid",_eSsid);
    ht.replace("##pwd",_showWifiPwd?_ePwd:"*****");                       // *****
    ht.replace("##ip",_eAdr[0]>0?_eAdr.toString():"");
    ht.replace("##gateway",_eGateway[0]>0?_eGateway.toString():"");
    ht.replace("##netmask",_eMask[0]>0?_eMask.toString():"255.255.255.0");
    String res=ht;
    res=htmlMask(res,false);
    if (_debug) Serial.println(res);
    request->send(200, "text/html", res);
    
  }

  String mHex(byte c)
  {
    char z[]="0123456789abcdef";
    return String(z[(byte)(c/16)])+String(z[(byte)(c%16)]);
  }

  String mHex8(unsigned long c)
  {
    char z[]="0123456789abcdef";
    String r="";
    for (int i=0;i<4;i++)
    {
      r=String(z[(byte)((c%256)/16)])+String(z[(byte)(c%16)])+r;
      c=c>>8;
    }
    return r;
  }

void BasicESP8266::_setserver()
{
  server = new AsyncWebServer(80);

//  server->on("/",HTTP_GET,[&](AsyncWebServerRequest *request) {_setup(request);});
 
  server->on("/setup",HTTP_GET,[&](AsyncWebServerRequest *request)
  {_setup(request);});
 
  
  server->on("/apsetup",HTTP_POST,[&](AsyncWebServerRequest *request)
  {
    if (_debug) DPRLN("\nSetup\n");
    _nextAPWifiCheck=millis()+10*_APWifiCheckIntervall;
    _setArgs(request);
    boolean apMode=_apFlag;
    String msg="";
    for (int i=0;i<(sizeof(apsnames)/sizeof(apsnames[0]));i++)
    {
      if (apsnames[i]=="pwd")
      {
        String val=request->getParam(apsnames[i],true)->value();
        msg+="pwd="+(val=="*****"?_ePwd:val)+"\n";
      }
      else msg+=apsnames[i]+"="+(request->hasParam(apsnames[i],true)?request->getParam(apsnames[i],true)->value():"")+"\n";
    }
    if (_debug) DPRLN (msg);
    saveFile("config",msg);
    _setConfig();
    _apFlag=0;
    _eePutULong(_IND_RESETCOUNT, (long)0);
    _eePutULong(_IND_APFLAG, (long)_apFlag);
    if (_debug) DPRLN ("Set RESETCOUNT to 0");
    String ip=_eAdr.toString();
    if (_debug) DPRLN("IP: "+ip);
    String h=ip.indexOf(".")>0?"<html><head></head><body>Restart device and click ok!<br><a href=\"http://"+ip+"\">ok</a></body></html>":
    "<html><head></head><body>Restart device and connect via DHCP given IP-Address (find out in your router)</body></html>";
// <meta http-equiv=\"refresh\" content=\"5; URL=http://"+ip+"\">    
    if (_debug) DPRLN(h);
    request->send(200, "text/html", h);
  });
  
  server->on("/info",HTTP_GET,[&](AsyncWebServerRequest *request)
  {
    String dir=_getSpiffs(true);
    String cinf=_chipInfo;
    cinf.replace("##real",String(realSize));
    cinf.replace("##ide",String(ideSize));
    cinf.replace("##mode",chipMode);
    cinf.replace("##total",String(totalSpiffs));
    cinf.replace("##used",String(usedSpiffs));
    cinf.replace("##free",String(freeSpiffs));
    String ht=wifiHtml1+wifiHtml2+wifiHtml3;
    ht.replace("##ssid",_eSsid);
    ht.replace("##pwd",_showWifiPwd?_ePwd:"*****");                       // *****
    ht.replace("##ip",_eAdr[0]>0?_eAdr.toString():"");
    ht.replace("##gateway",_eGateway[0]>0?_eGateway.toString():"");
    ht.replace("##netmask",_eMask[0]>0?_eMask.toString():"255.255.255.0");
    String res=dir+"<br>"+cinf+ht;
    res=htmlMask(res,false);
    if (_debug) Serial.println(res);
    request->send(200, "text/html", res);
    
  });

  server->on( "/upload", HTTP_POST, [&]( AsyncWebServerRequest * request )
  {},
  [&]( AsyncWebServerRequest * request, String filename, size_t index, uint8_t *data, size_t len, bool final )
  {
    if ( !index )
    {
      if(_debug) Serial.printf( "UPLOAD: Started to receive '%s'.\n", filename.c_str() );
      f = LittleFS.open("/"+filename, "w+");
    }
    for (int i=0;i<len;i++) f.write((char)data[i]);
    if ( final )
    {
      f.close();
      if (_debug) Serial.printf( "UPLOAD: Done. Received %ld Bytes.\n", index);
      request->send(200, "text/html", "\nUPLOAD: Done. Received "+String(index)+" Bytes.\n");
    }
  });


  
  server->onNotFound([](AsyncWebServerRequest *request) {
      request->send(404, "text/plain", "Not found");
   });  

  server->on("/mem", HTTP_GET, [&] (AsyncWebServerRequest *request) 
  {
    String fn="";
    String res="";
    if (request->hasParam("filename")) 
    {
      fn = request->getParam("filename")->value();
      if (fn.charAt(0)!='/')fn="/"+fn;
      f= LittleFS.open(fn,"r");
      if(f)
      {
        if (_debug) Serial.println("Serving file \""+fn+"\"");    
        if (fn=="/config")
        {
          String fc=loadFile(fn);
          String pw=getInitValue(fc,"pwd");
          if (!_showWifiPwd) fc.replace("pwd="+pw,"pwd=*****"); 
          res=fn+":\n\n"+fc;
        }
        else
        {
          request->send(f, fn, "application/octet-stream");
          return;
        }
      } 
      else res="File "+fn+" not found"; 
      res=htmlMask(res,true);
    } 
    else if (request->hasParam("delete")) 
    {
      fn = request->getParam("delete")->value();
      res = _deleteFile(fn)? "File "+fn+" deleted":"File "+fn+" could not be deleted";
      res=htmlMask(res,true);
    } 
    else 
    {
      res=htmlMask(_getSpiffs(true),false);
    }
    request->send(200, "text/html", res);
  });

  server->on("/dump", HTTP_GET, [&] (AsyncWebServerRequest *request)
  {
    _fn="";
    if (request->hasParam("filename")) 
    {
      _fn = request->getParam("filename")->value();
      if (_fn.charAt(0)!='/') _fn="/"+_fn;
      _dumpFormat=0;
      if (request->hasParam("format"))
      {
        String df=request->getParam("format")->value();
        if (df=="json") _dumpFormat=2; 
        if (df=="html") _dumpFormat=1;
      }
         _n=0;
        _pos=0;
        String type="text/plain";
        if (_dumpFormat==1) type="text/html";
        if (_dumpFormat==2) type="application/json";
        f=LittleFS.open(_fn,"r");
        if (!f || (_fn=="/config" && !_showWifiPwd)) request->send(200, "text/plain", "File not available");
        else
        {
          AsyncWebServerResponse *response = request->beginChunkedResponse(type, [&](uint8_t *buffer, size_t maxLen, size_t index) -> size_t 
          {
            char bu[33]="                ";
            bu[32]=0;
            String r="";
            if (!f.available()) return 0;
            if (_pos==0)
            {
              if (_dumpFormat==0) r+=_fn+"\n\n";
              if (_dumpFormat==1) r+="<!DOCTYPE html>\n<html><body><pre>\n"+_fn+"\n\n";
              if (_dumpFormat==2) r+="{\"filename\":\""+_fn+"\",\"data\":[";
            }
            if (_pos>0)
            {
              if (_dumpFormat==0) r+="\n";
              if (_dumpFormat==1) r+="\n";
              if (_dumpFormat==2) r+=",";
            }
       
            r+=(_dumpFormat==2?"\"":"")+mHex8(_pos)+": ";
            int n=0;
            while (f.available() && n<32)
            {
              byte c=(byte)f.read();
              r+=mHex(c)+(n%2==1?" ":"");
              bu[n]=(c>32 && c<127 && c!=34 && c!=92)?c:46;
              n++;
              _pos++;
              if (n==32) r+="    "+String(bu)+(_dumpFormat==2?"\"":"");
            }
            if (n%32!=0)
            {
              for(int i=n%32;i<32;i++)
              {
                r+="   ";
                bu[i]=46;
              }
              r+="    "+String(bu)+(_dumpFormat==2?"\"":"");
            }
            if (!f.available())
            {
              if (_dumpFormat==0) r+="\n";
              if (_dumpFormat==1) r+="\n</pre></body></html>\n";
              if (_dumpFormat==2) r+="]}";
            }  

            int len=r.length();
            if (len<maxLen) memcpy(buffer, (uint8_t *)r.c_str(),len);
            else len=0;
            buffer[len]=0;
            _n++;
            return len;
          });
          request->send(response);
        
      }
    }
    else request->send(200,"text/plain", "use <ip>/dump?filename=<filename>");
  });
    
  
  server->begin();
  if (_debug) DPRLN("Server started");
}

void BasicESP8266::loop()
{

  if (!apmode) 
  {
    ArduinoOTA.handle();
  }
  else
  {
    if (_sigOff==0) {_sigOn=millis()+5000;_sigOff=_sigOn+500;}
  }

  if (_resetCount>0 && millis()>_RESETTIME) _eePutULong(_IND_RESETCOUNT, (long)0);


  if (_sigOn>0 && millis()>_sigOn)
  {
    digitalWrite(_sigLed,_sigLowActive?LOW:HIGH);
    _sigOn=0;
    _sigOff=millis()+_sigOnDur;
  }
  
  if (_sigOff>0 && millis()>_sigOff)
  {
    digitalWrite(_sigLed,_sigLowActive?HIGH:LOW);
    _sigOff=0;
    if (_sigCount>0)
    {
      _sigOn=millis()+_sigOffDur;
      _sigCount--;
    }
  }
}