#include <ESPClock.h>
#include <time.h>

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
    JsonDocument clockConfigJson;

    if(LittleFS.exists(_configFileName)) {
        configFile = LittleFS.open(_configFileName, "r");
        deserializeJson(clockConfigJson, configFile);
        configFile.close();
    } else {
        clockConfigJson["brightness"] = _clockConfig.brightness;
        clockConfigJson["blink"] = _clockConfig.blink;
        clockConfigJson["alarmtime"] = _clockConfig.alarmTime;
        clockConfigJson["alarmactive"] = _clockConfig.alarmActive;
        clockConfigJson["twelvehours"] = _clockConfig.twelveHours;
        clockConfigJson["tzstring"] = _clockConfig.tzString;

        configFile = LittleFS.open(_configFileName, "w");
        serializeJson(clockConfigJson, configFile);
        configFile.close();
    }

    _applyClockConfigFromJson(clockConfigJson);
    _display.clear();
    _setEndPoints();
    _setupClock();
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
        if(timeinfo.tm_sec == 0) {
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

    if(_clockConfig.twelveHours){
        hours = hours > 12 ? hours - 12 : hours;
    }

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
        JsonDocument status;
        String command = request->url();
        String response;

        if(command.endsWith("on")) {
            if(!_alarmOn) {
                _alarmOn = true;
                _displayState = ON;
                _displayStartTime = millis();
            } else

                status["error"] = "Alarm is already on";

            } else if(command.endsWith("off")) {
                if(_alarmOn) {
                    _alarmOn = false;
                    _displayState = OFF;
                    _displayStartTime = millis();
                } else

                status["error"] = "Alarm is already off";

        } else if(command.endsWith("status"))
            ;
        else if(!command.endsWith("status"))
            status["error"] = "No valid command found. Must be on, off or status. Sending status.";

        status["alarmon"] = _alarmOn;
        serializeJson(status, response);
        request->send(200, "text/json", response);
    });

    _esp.server->on("/currenttime", HTTP_GET, [&](AsyncWebServerRequest *request) {
        JsonDocument jsonResponse;
        String response;
        uint16_t timenow = timeinfo.tm_hour * 100 + timeinfo.tm_min;
        jsonResponse["currenttime"] = timenow;
        serializeJson(jsonResponse, response);
        request->send(200, "text/json", response);
    });

    AsyncCallbackJsonWebHandler* handler = new AsyncCallbackJsonWebHandler("/clockconfig", [this](AsyncWebServerRequest *request, JsonVariant &json) {
        if(_debug) Serial.println("Getting clockconfig request");
        JsonObject newClockConfig;
        String response = "";
        File configFile;

        if(request->method() == HTTP_POST && json != NULL) {
            if(_debug) Serial.println("Reading in new clockconfig data");

            newClockConfig = json.as<JsonObject>();
            bool timeChange = false;

            if(newClockConfig["brightness"].is<int>() &&  newClockConfig["brightness"] != _clockConfig.brightness) {
                _clockConfig.brightness = newClockConfig["brightness"];
                _display.setBrightness(_clockConfig.brightness);
            }

            if(newClockConfig["blink"].is<bool>() && newClockConfig["blink"] != _clockConfig.blink)
                _clockConfig.blink = newClockConfig["blink"];

            if(newClockConfig["alarmtime"].is<uint16_t>() && newClockConfig["alarmtime"] != _clockConfig.alarmTime)
                _clockConfig.alarmTime = newClockConfig["alarmtime"];

            if(newClockConfig["alarmactive"].is<bool>() && newClockConfig["alarmactive"] != _clockConfig.alarmActive)
                _clockConfig.alarmActive = newClockConfig["alarmactive"];

            if(newClockConfig["twelvehours"].is<bool>() && newClockConfig["twelvehours"] != _clockConfig.twelveHours)
                _clockConfig.twelveHours = newClockConfig["twelvehours"];

            if(newClockConfig["tzstring"].is<const char*>() && newClockConfig["tzstring"] != _clockConfig.tzString) {
                _clockConfig.tzString = newClockConfig["tzstring"];
                timeChange = true;
                if(_debug)
                    Serial.printf("Setting up new timezone in clockConfig: %s\n", _clockConfig.tzString);
            }

            if(timeChange)
                _setTZString();

            serializeJson(newClockConfig, response);

        } else if(request->method() == HTTP_PUT && json != NULL) {
            JsonDocument oldClockConfig;
            JsonDocument responseJson;
            configFile = LittleFS.open(_configFileName, "r");
            deserializeJson(oldClockConfig, configFile);

            if(oldClockConfig["brightness"] != _clockConfig.brightness
                || oldClockConfig["blink"] != _clockConfig.blink
                || oldClockConfig["alarmtime"] != _clockConfig.alarmTime
                || oldClockConfig["alarmactive"] != _clockConfig.alarmActive
                || oldClockConfig["twelvehours"] != _clockConfig.twelveHours
                || oldClockConfig["tzstring"] != _clockConfig.tzString) {

                if(_debug) Serial.println("Writing new clockconfig data");

                configFile = LittleFS.open(_configFileName, "w");
                serializeJson(_createJsonFromClockConfig(), configFile);
                configFile.close();
                responseJson["persist"] = true;
            } else {
                responseJson["persist"] = false;
            }

            serializeJson(responseJson, response);

        } else if(request->method() == HTTP_GET) {
            serializeJson(_createJsonFromClockConfig(), response);

            if(_debug) {
                Serial.println("Sending current clockconfig data:");
                Serial.println(response);
            }
        }

        request->send(200, "application/json", response);
    });

    _esp.server->addHandler(handler);
}

void ESPClock::_applyClockConfigFromJson(JsonDocument clockConfigJson) {
    _clockConfig.brightness = clockConfigJson["brightness"];
    _display.setBrightness(_clockConfig.brightness);
    _clockConfig.blink = clockConfigJson["blink"];
    _clockConfig.alarmTime = clockConfigJson["alarmtime"];
    _clockConfig.alarmActive = clockConfigJson["alarmactive"];
    _clockConfig.twelveHours = clockConfigJson["twelvehours"];
    _clockConfig.tzString = clockConfigJson["tzstring"];
}

JsonDocument ESPClock::_createJsonFromClockConfig() {
    JsonDocument document;

    document["blink"] = _clockConfig.blink;
    document["brightness"] = _clockConfig.brightness;
    document["alarmactive"] = _clockConfig.alarmActive;
    document["alarmtime"] = _clockConfig.alarmTime;
    document["twelvehours"] = _clockConfig.twelveHours;
    document["tzstring"] = _clockConfig.tzString;

    if(_debug)
        Serial.printf("Creating json from clockConfig. tzstring=%s\n", _clockConfig.tzString);

    return document;
}

void ESPClock::_setupClock() {
    configTime("CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "pool.ntp.org");
    getLocalTime(&timeinfo);

    if(_debug)
        Serial.printf("Time: %d:%d\n", timeinfo.tm_hour, timeinfo.tm_min);
}

void ESPClock::_setTZString() {
    setenv("TZ", _clockConfig.tzString, 1);
    tzset();
    time_t now = time(nullptr);
//    *timeinfo = localtime(&now);
    if(_debug)
        Serial.printf("Setting timezone to %s\n", _clockConfig.tzString);
}