#include <ESPClock.h>
#include <TimeLib.h>
#include <ArduinoJson.h>

ESPClock::ESPClock(bool debug, int dio_pin, int clk_pin, int button_pin, int buzzer_pin)
    : _esp(debug, 100, true, false, false), _button(button_pin, true, false), _display(clk_pin, dio_pin) {
    _debug = debug;
    _buzzer_pin = buzzer_pin;
    pinMode(_buzzer_pin, OUTPUT);

    _button.attachClick([](void *ctx){
        ((ESPClock*)ctx)->handleClick();
    }, this);

    _button.attachLongPressStop([](void *ctx) {
        ((ESPClock*)ctx)->handleLongPress();
    }, this);

    _esp.begin();
    _count = 0;
    _buzzer_state = LOW;

    File file = LittleFS.open("/brightness", "r");

    if(file != -1) {
        JsonDocument doc;
        deserializeJson(doc, file);

        if(doc["level"].is<int>() && doc["level"].as<int>() >= 0 && doc["level"].as<int>() <= 7) {
            _brightness = doc["level"].as<int>();
        }
    }

    _display.setBrightness(_brightness);
    _display.clear();
    _setEndPoints();
}

void ESPClock::button_tick() {
    _button.tick();
}

void ESPClock::_handleAlarm() {
    if(_alarmOn) {
        if(_buzzer_state == LOW) {
            _buzzer_state = HIGH;
        } else {
            _buzzer_state = LOW;
        }

        digitalWrite(_buzzer_pin, _buzzer_state);
    }
}

void ESPClock::displayTime() {
    uint32_t current_time = now();
    uint32_t time_to_check = current_time - _esp.getUpdateInterval() / 1000;
    int hours = (current_time % 86400L) / 3600;
    int minutes = (current_time % 3600) / 60;

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

        if(!_alarmOn && _alarmTime / 100 == hours && _alarmTime % 100 == minutes) {
            if(_debug) Serial.println("Turning on alarm");
            _alarmOn = true;
            _buzzer_state = HIGH;
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

    uint8_t clock_data[4];
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

    _handleAlarm();
}

void ESPClock::handleClick() {

    if(_debug) Serial.println("Button clicked");
    if(_alarmOn) {
        if(_debug) Serial.println("Turning off alarm");
        _alarmOn = false;
        _buzzer_state = LOW;
        _button_presses++;
        digitalWrite(_buzzer_pin, _buzzer_state);
    } else {
        _buzzer_state = !_buzzer_state;
        digitalWrite(_buzzer_pin, _buzzer_state);
    }
}

void ESPClock::handleLongPress() {
    int hours = _alarmTime / 100;
    int minutes = _alarmTime % 100;
    uint8_t clock_data[4];
    clock_data[0] = hours >= 10 ? _display.encodeDigit(hours / 10) : 0;
    clock_data[1] = _display.encodeDigit(hours % 10) | 128;
    clock_data[2] = _display.encodeDigit(minutes / 10);
    clock_data[3] = _display.encodeDigit(minutes % 10);

    _display.setSegments(clock_data);
    delay(3000);
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

    _esp.server->on("/brightness", HTTP_GET, [&](AsyncWebServerRequest *request) {
        JsonDocument doc;

        if (request->hasParam("level")) {
            int newBrightness = request->getParam("level")->value().toInt();
            File file = LittleFS.open("/brightness", "r");

            if(file != -1) {
                deserializeJson(doc, file);
                doc["level"] = newBrightness;
                
                if(_brightness != newBrightness) {
                    file = LittleFS.open("/brightness", "w");
                    serializeJson(doc, file);
                }
            } else {
                file = LittleFS.open("/brightness", "w");

                if(file != -1) {
                    doc["level"] = newBrightness;
                    serializeJson(doc, file);
                }
            }

            _display.setBrightness(newBrightness);
            _brightness = newBrightness;
        } else {
            doc["level"] = _brightness;
        }

        String response;
        serializeJson(doc, response);
        request->send(200, "text/json", response);
    });

    _esp.server->on("/blink/on", HTTP_GET, [&](AsyncWebServerRequest *request) {
        _blink = true;
        request->send(200, "text/pain", "Turning blinking colons on");
    });

    _esp.server->on("/blink/off", HTTP_GET, [&](AsyncWebServerRequest *request) {
        _blink = false;
        request->send(200, "text/pain", "Turning blinking colons off");
    });

    _esp.server->on("/test", HTTP_GET, [&](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Test response from ESPClock");
    });

    _esp.server->on("/alarm", HTTP_GET, [&](AsyncWebServerRequest *request) {
        if(request->hasParam("time")) {
            uint16_t alarmTime = request->getParam("time")->value().toInt();
            _alarmTime = alarmTime <= 2359 ? alarmTime : 600;
        }

        if(request->hasParam("active")) {
            if(request->getParam("active")->value().equals("true"))
                _alarmActive = true;
            else
                _alarmActive = false;
        }

        char response[22] = "Set alarm time: ";
        char alarmTimeStr[6];
        itoa(_alarmTime, alarmTimeStr, 10);

        strcat(response, alarmTimeStr);

        request->send(200, "text/plain", response);
    });

    _esp.server->on("/alarmoff", HTTP_GET, [&](AsyncWebServerRequest *request) {
        _alarmOn = false;
        _buzzer_state = LOW;
        digitalWrite(_buzzer_pin, _buzzer_state);

        request->send(200, "text/plain", "turned off alarm");
    });

    _esp.server->on("/isactive", HTTP_GET, [&](AsyncWebServerRequest *request) {
        if(_alarmActive)
            request->send(200, "text/plain", "Alarm is active\n");
        else
            request->send(200, "text/plain", "Alarm is not active\n");
    });

    _esp.server->on("/alarmtime", HTTP_GET, [&](AsyncWebServerRequest *request) {
        char alarmTimeStr[6];
        itoa(_alarmTime, alarmTimeStr, 10);
        request->send(200, "text/plain", alarmTimeStr);
    });

    _esp.server->on("/alarmstatus", HTTP_GET, [&](AsyncWebServerRequest *request) {
        if(_alarmOn)
            request->send(200, "text/plain", "Alarm is on\n");
        else
            request->send(200, "text/plain", "Alarm is off\n");
    });

    _esp.server->on("/buttonpresses", HTTP_GET, [&](AsyncWebServerRequest *request) {
        char numpresses[6];
        itoa(_button_presses, numpresses, 10);
        request->send(200, "text/plain", numpresses);
    });

    _esp.server->on("/currenttime", HTTP_GET, [&](AsyncWebServerRequest *request) {
        uint32_t current_time = now();
        int hours = (current_time % 86400L) / 3600;
        int minutes = (current_time % 3600) / 60;
        char timeStr[7];
        timeStr[0] = hours / 10 + 48;
        timeStr[1] = hours % 10 + 48;
        timeStr[2] = ':';
        timeStr[3] = minutes / 10 + 48;
        timeStr[4] = minutes % 10 + 48;
        timeStr[5] = '\n';
        timeStr[6] = '\0';

        request->send(200, "text/plain", timeStr);
    });
}

char *ESPClock::_getAlarmTimeStr() {
        char alarmTimeStr[6];
        int minute = _alarmTime % 60;
        char minuteStr[2];
        itoa(minute, minuteStr, 2);
        int hour = _alarmTime / 100;
        char hourStr[2];
        itoa(hour, hourStr, 2);
        strcat(alarmTimeStr, hourStr);
        strcat(alarmTimeStr, ":");
        strcat(alarmTimeStr, minuteStr);
        alarmTimeStr[5] = '\0';

        return alarmTimeStr;
}