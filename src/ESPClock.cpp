#include <ESPClock.h>
#include <time.h>
#include "jsonlib.h"

ESPClock::ESPClock(bool debug, int dio_pin, int clk_pin, int button_pin, int buzzer_pin)
    : _esp(debug, 100, true, false, false), _button(button_pin, true, false), _display(clk_pin, dio_pin) {
    _debug = debug;
    _buzzer_pin = buzzer_pin;
    pinMode(_buzzer_pin, OUTPUT);

    _button.attachClick([](void *ctx){
        ((ESPClock*)ctx)->_handleClick();
    }, this);

    _button.attachLongPressStop([](void *ctx) {
        ((ESPClock*)ctx)->_handleLongPress();
    }, this);

    _esp.begin();
    _count = 0;
    _buzzer_state = LOW;

    File configFile;

    if(LittleFS.exists(_configFileName)) {
        configFile = LittleFS.open(_configFileName, "r");
        String configContent = configFile.readString();
        _addToClockConfigFromJson(configContent);
    } else {
        _saveClockConfig();
    }

    _display.setBrightness(_clockConfig.brightness);
    _display.clear();
    _setupClock();
    _setEndPoints();
    _displayState = CLOCK;
}

void ESPClock::button_tick() {
    _button.tick();
}

void ESPClock::loop() {
    _esp.loop();
}

void ESPClock::_handleAlarm() {
    if(_alarmOn) {
        if(_buzzer_state == LOW)
            _buzzer_state = HIGH;
        else
            _buzzer_state = LOW;

        digitalWrite(_buzzer_pin, _buzzer_state);
    } else {
        if(_buzzer_state == HIGH)
            digitalWrite(_buzzer_pin, LOW);
    }
}

void ESPClock::doDisplay() {
    switch(_displayState) {
        case CLOCK: {
            _displayTime();
        } break;

        case ALARMTIME: {
            _displayAlarmTime();
        } break;

        case ON: {
            _displayString("on");
        } break;

        case OFF: {
            _displayString("off");
        } break;
    }

    _handleAlarm();
}

void ESPClock::_displayTime() {
    getLocalTime(&timeinfo);
    uint8_t hours = timeinfo.tm_hour;
    uint8_t minutes = timeinfo.tm_min;
    uint32_t current_time = hours * 10000 + minutes * 100 + timeinfo.tm_sec;

    if(_previousTime != current_time) {
        if(current_time % 60 == 0) {
            if(_debug) {
                Serial.println("At the minute mark");
                Serial.print("_alarmTime / 100=");
                Serial.println(_clockConfig.alarmTime / 100);
                Serial.print("_alarmTime % 100=");
                Serial.println(_clockConfig.alarmTime % 100);
                Serial.print("Hours:");
                Serial.println(hours);
                Serial.print("Minutes:");
                Serial.println(minutes);
            }

            if(_clockConfig.alarmActive && !_alarmOn && _clockConfig.alarmTime / 100 == hours && _clockConfig.alarmTime % 100 == minutes) {
                if(_debug) Serial.println("Turning on alarm");
                _alarmOn = true;
                _buzzer_state = HIGH;
                _displayState = ON;
                _displayStartTime = millis();
            }
        }
    }

    uint8_t clock_data[4];

    if(_clockConfig.twelveHours)
        hours = hours > 12 ? hours - 12 : hours;

    clock_data[0] = hours >= 10 ? _display.encodeDigit(hours / 10) : 0;
    clock_data[1] = _display.encodeDigit(hours % 10) | _showColon;
    clock_data[2] = _display.encodeDigit(minutes / 10);
    clock_data[3] = _display.encodeDigit(minutes % 10);

    _display.setSegments(clock_data);

    if(_clockConfig.blink) {
        _showColon = _showColon == 128 ? 0 : 128;
    } else {
        _showColon = 128;
    }

    _previousTime = current_time;
}

void ESPClock::_displayAlarmTime() {
    uint32_t now = millis();

    if(now < _displayStartTime + _displayDuration) {
        int hours = _clockConfig.alarmTime / 100;
        int minutes = _clockConfig.alarmTime % 100;

        if(_clockConfig.twelveHours)
            hours = hours > 12 ? hours - 12 : hours;

        uint8_t clock_data[4];
        clock_data[0] = hours >= 10 ? _display.encodeDigit(hours / 10) : 0;
        clock_data[1] = _display.encodeDigit(hours % 10) | 128;
        clock_data[2] = _display.encodeDigit(minutes / 10);
        clock_data[3] = _display.encodeDigit(minutes % 10);

        if(_debug) Serial.println("Displaying alarm time");

        _display.setSegments(clock_data);

    } else
        _displayState = CLOCK;
}

void ESPClock::_displayString(String str) {
    uint32_t now = millis();

    if(now < _displayStartTime + _displayDuration) {
        uint8_t data[4] = { 0 };

        if(str == "on") {
            if(_debug) Serial.println("Displaying On");

            data[0] = _display.encodeDigit(0);
            data[1] = SEG_C | SEG_E | SEG_G;
        } else if(str == "off") {
            if(_debug) Serial.println("Displaying Off");

            data[0] = _display.encodeDigit(0);
            data[1] = SEG_A | SEG_E | SEG_F | SEG_G;
            data[2] = data[1];
        }

        _display.setSegments(data);
    } else
        _displayState = CLOCK;
}

void ESPClock::_handleClick() {
    if(_debug) Serial.println("Button clicked");
    if(_alarmOn) {
        if(_debug) Serial.println("Turning off alarm");
        _alarmOn = false;
        _displayState = OFF;
        _displayStartTime = millis();
    }
}

void ESPClock::_handleLongPress() {
    _displayState = ALARMTIME;
    _displayStartTime = millis();
}

void ESPClock::_setEndPoints() {
    _esp.server->on("/clock", HTTP_GET, [&](AsyncWebServerRequest *request) {
            File f = LittleFS.open("/index.html", "r");

            if (!f) {
                if (_debug) Serial.println("no index.html");
                request->send(404, "text/plain", "No index.html file");
                return;
            }

            request->send(LittleFS, "/index.html", "text/html");
    });

    _esp.server->on("/alarm/*", HTTP_GET, [&](AsyncWebServerRequest *request) {
        String status;
        String command = request->url();
        String response;

        if(command.endsWith("on")) {
            if(!_alarmOn) {
                _alarmOn = true;
                _displayState = ON;
                _displayStartTime = millis();
            } else

                status = "{\"error\":\"Alarm is already on\"";

            } else if(command.endsWith("off")) {
                if(_alarmOn) {
                    _alarmOn = false;
                    _displayState = OFF;
                    _displayStartTime = millis();
                } else

                status = "{\"error\":\"Alarm is already off\"";


        } else if(command.endsWith("status"))
            ;
        else if(!command.endsWith("status"))
            status = "{\"error\":\"No valid command found. Must be on, off or status. Sending status.\"";

        status += ",\"alarmn\":" + String(_alarmOn) + "}";
        request->send(200, "application/json", status);
    });

    _esp.server->on("/currenttime", HTTP_GET, [&](AsyncWebServerRequest *request) {
        String response;
        uint16_t timenow = timeinfo.tm_hour * 100 + timeinfo.tm_min;;
        response = "{\"currenttime\":" + String(timenow) + "}";
        request->send(200, "application/json", response);
    });

    
    _esp.server->on("/clockconfig", HTTP_GET, [&](AsyncWebServerRequest *request) {
        String response;
        response = _getClockConfigJson(false, false);
        request->send(200, "application/json", response);
    });

    _esp.server->on("/clockconfig", HTTP_POST, [](AsyncWebServerRequest *request){},
                    NULL, [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
        String response;
        String body;
        ClockConfig incomingConfig;
        if(len > 0) {
            body = String((const char *) data);
            _addToClockConfigFromJson(body);
        }
        _applyClockConfig();
        response = _getClockConfigJson(false, false);
        request->send(200, "application/json", response);
    });

/*    _esp.server->on("/clockconfig", HTTP_ANY, [](AsyncWebServerRequest *request) {}, 
                    NULL, 
                    [this](AsyncWebServerRequest * request, uint8_t *data, size_t len, size_t index, size_t total) {
        String response;
        String body;
        ClockConfig incomingConfig;

        if(_debug)
            Serial.println("Received clockconfig request");

        if(len > 0) {
            body = String((const char *) data);
            incomingConfig = _createClockConfigFromJson(body);
        }

        if(request->method() == HTTP_GET) {
            response = _getClockConfigJson(false, false);
        } else if(request->method() == HTTP_POST) {
            _clockConfig = incomingConfig;
            _applyClockConfig();
            response = _getClockConfigJson(false, false);
        } else if(request->method() == HTTP_PUT) {
            if(incomingConfig.alarmActive != _clockConfig.alarmActive
                    || incomingConfig.alarmTime != _clockConfig.alarmTime
                    || incomingConfig.blink != _clockConfig.blink
                    || incomingConfig.brightness != _clockConfig.brightness
                    || incomingConfig.dst != _clockConfig.dst
                    || incomingConfig.twelveHours != _clockConfig.twelveHours>
                    || incomingConfig.tzOffset != _clockConfig.tzOffset) {
                _clockConfig = incomingConfig;
                _applyClockConfig();
                _saveClockConfig();
                response = _getClockConfigJson(true, true);
            } else {
                response = _getClockConfigJson(false, true);
            }
        } else {
            response = "{\"error\":\"Invalid Request\"}";
        }

        request->send(200, "application/json", response);
    });*/
}

void ESPClock::_applyClockConfig() {
    _display.setBrightness(_clockConfig.brightness);
    setTZ(_clockConfig.tzString.c_str());
}

void ESPClock::_addToClockConfigFromJson(String json) {
    ClockConfig configFromJson;

    String alarmActive = jsonExtract(json, "alarmactive");
    if(!alarmActive.isEmpty()) _clockConfig.alarmActive = alarmActive.equals("true");

    String alarmTime = jsonExtract(json, "alarmtime");
    if(!alarmTime.isEmpty()) _clockConfig.alarmTime = alarmTime.toInt();

    String blink = jsonExtract(json, "blink");
    if(!blink.isEmpty()) _clockConfig.blink = blink.equals("true");

    String brightness = jsonExtract(json, "brightness");
    if(!brightness.isEmpty()) _clockConfig.brightness = brightness.toInt();

    String twelveHours = jsonExtract(json, "twelvehours");
    if(!twelveHours.isEmpty()) _clockConfig.twelveHours = twelveHours.equals("true");

    String tzString = jsonExtract(json, "tzstring");
    if(!tzString.isEmpty()) _clockConfig.tzString = tzString;
}

void ESPClock::_saveClockConfig() {
    File configFile = LittleFS.open("/clockconfig.json", "w");
    configFile.print(_getClockConfigJson(false, false).c_str());
    configFile.close();
}

String ESPClock::_getClockConfigJson(bool persist, bool showPersist) {
    String result =  "{\"alarmactive\":" + _getBoolString(_clockConfig.alarmActive) + ",\"alarmtime\":" + String(_clockConfig.alarmTime)
        + ",\"blink\":" + _getBoolString(_clockConfig.blink) + ",\"brightness\":" + String(_clockConfig.brightness) + 
        + ",\"tzstring\":" + String(_clockConfig.tzString);

    if(showPersist) {
        result += ",\"persist\":" + _getBoolString(persist);
    }

    result += "}";

    return result;
}

String ESPClock::_getBoolString(bool var) {
    if(var)
        return "true";

    return "false";
}

void ESPClock::_setupClock() {
    configTime(_clockConfig.tzString.c_str(), "pool.ntp.org");
    getLocalTime(&timeinfo);

    if(_debug)
        Serial.printf("Time: %d:%d\n", timeinfo.tm_hour, timeinfo.tm_min);
}