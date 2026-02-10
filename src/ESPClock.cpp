#include <ESPClock.h>
#include <TimeLib.h>

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

    if(LittleFS.exists("/clockconfig.json")) {
        configFile = LittleFS.open("/clockconfig.json", "r");
        deserializeJson(_clockConfig, configFile);
    } else {
        _clockConfig["brightness"] = _brightness;
        _clockConfig["blink"] = _blink;
        _clockConfig["alarmtime"] = _alarmTime;
        _clockConfig["alarmactive"] = _alarmActive;
        _clockConfig["twelvehours"] = _twelveHours;

        configFile = LittleFS.open("/clockconfig.json", "w");
        serializeJson(_clockConfig, configFile);
        configFile.close();
    }

    _applyClockConfig();
    _display.clear();
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
    uint32_t current_time = now();

    uint32_t time_to_check = current_time - _esp.getUpdateInterval() / 1000;
    int hours = (current_time % 86400L) / 3600;
    int minutes = (current_time % 3600) / 60;

    if(_previousTime != current_time) {
        if(current_time % 60 == 0) {
            if(_debug) {
                Serial.println("At the minute mark");
                Serial.print("_alarmTime / 100=");
                Serial.println(_alarmTime / 100);
                Serial.print("_alarmTime % 100=");
                Serial.println(_alarmTime % 100);
                Serial.print("Hours:");
                Serial.println(hours);
                Serial.print("Minutes:");
                Serial.println(minutes);
            }

            if(_alarmActive && !_alarmOn && _alarmTime / 100 == hours && _alarmTime % 100 == minutes) {
                if(_debug) Serial.println("Turning on alarm");
                _alarmOn = true;
                _buzzer_state = HIGH;
                _displayState = ON;
                _displayStartTime = millis();
            }
        } else if(_lastUpdated < time_to_check || _lastUpdated == 0) {
            uint32_t ntp_time = _esp.getEpochTime();
            setTime(ntp_time);
            _lastUpdated = current_time;

            if(_debug) {
                Serial.print("Retrieved time from NTP server: ");
                Serial.print((current_time % 86400L) / 3600);
                Serial.print(":");
                Serial.print((current_time % 3600) / 60 / 10);
                Serial.print(((current_time % 3600) / 60) % 10);
            }
        }
    }

    uint8_t clock_data[4];

    if(_twelveHours)
        hours = hours > 12 ? hours - 12 : hours;

    clock_data[0] = hours >= 10 ? _display.encodeDigit(hours / 10) : 0;
    clock_data[1] = _display.encodeDigit(hours % 10) | _showColon;
    clock_data[2] = _display.encodeDigit(minutes / 10);
    clock_data[3] = _display.encodeDigit(minutes % 10);

    _display.setSegments(clock_data);

    if(_blink) {
        _showColon = _showColon == 128 ? 0 : 128;
    } else {
        _showColon = 128;
    }

    _previousTime = current_time;
}

void ESPClock::_displayAlarmTime() {
    uint32_t now = millis();

    if(now < _displayStartTime + _displayDuration) {
        int hours = _alarmTime / 100;
        int minutes = _alarmTime % 100;

        if(_twelveHours)
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
        uint32_t current_time = now();
        int hours = (current_time % 86400L) / 3600;
        int minutes = (current_time % 3600) / 60;
        uint16_t timenow = hours * 100 + minutes;
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
            int changes = 0;

            if(newClockConfig["brightness"].is<int>() &&  newClockConfig["brightness"] != _brightness){
                _brightness = newClockConfig["brightness"];
                _clockConfig["brightness"] = _brightness;
                changes++;
            }

            if(newClockConfig["blink"].is<bool>() && newClockConfig["blink"] != _blink){
                _blink = newClockConfig["blink"];
                _clockConfig["blink"] = _blink;
                changes++;
            }

            if(newClockConfig["alarmtime"].is<uint16_t>() && newClockConfig["alarmtime"] != _alarmTime){
                _alarmTime = newClockConfig["alarmtime"];
                _clockConfig["alarmtime"] = _alarmTime;
                changes++;
            }

            if(newClockConfig["alarmactive"].is<bool>() && newClockConfig["alarmactive"] != _alarmActive){
                _alarmActive = newClockConfig["alarmactive"];
                _clockConfig["alarmactive"] = _alarmActive;
                changes++;
            }

            if(newClockConfig["twelvehours"].is<bool>() && newClockConfig["twelvehours"] != _twelveHours){
                _twelveHours = newClockConfig["twelvehours"];
                _clockConfig["twelvehours"] = _twelveHours;
                changes++;
            }

            _applyClockConfig();
            serializeJson(newClockConfig, response);

        } else if(request->method() == HTTP_PUT && json != NULL) {
            JsonDocument oldClockConfig;
            JsonDocument responseJson;
            configFile = LittleFS.open("/clockconfig.json", "r");
            deserializeJson(oldClockConfig, configFile);

            if(oldClockConfig["brightness"] != _clockConfig["brightness"]
                || oldClockConfig["blink"] != _clockConfig["blink"]
                || oldClockConfig["alarmtime"] != _clockConfig["alarmtime"]
                || oldClockConfig["alarmactive"] != _clockConfig["alarmactive"]
                || oldClockConfig["twelvehours"] != _clockConfig["twelvehours"]) {
                if(_debug) Serial.println("Writing new clockconfig data");

                configFile = LittleFS.open("/clockconfig.json", "w");
                serializeJson(_clockConfig, configFile);
                configFile.close();
                responseJson["persist"] = true;
            } else {
                responseJson["persist"] = false;
            }

            serializeJson(responseJson, response);

        } else if(request->method() == HTTP_GET) {
            serializeJson(_clockConfig, response);

            if(_debug) {
                Serial.println("Sending current clockconfig data:");
                Serial.println(response);
            }
        }

        request->send(200, "application/json", response);
    });

    _esp.server->addHandler(handler);
}

void ESPClock::_applyClockConfig() {
    _brightness = _clockConfig["brightness"];
    _display.setBrightness(_brightness);
    _blink = _clockConfig["blink"];
    _alarmTime = _clockConfig["alarmtime"];
    _alarmActive = _clockConfig["alarmactive"];
    _twelveHours = _clockConfig["twelvehours"];
}