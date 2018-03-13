#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "WirelessInput.h"


//Return JSON of AppData1 content
String AppData1::GetJSON() {
  
  char fpStr[60];
  String gc;

  gc = gc + F("\"inv\":\"") + (invert ? F("on") : F("off")) + '"';
  //there is a predefined special password (mean to keep already saved one)
  gc = gc + F(",\"je\":\"") + (jeedom.enabled ? F("on") : F("off")) + F("\",\"jt\":\"") + (jeedom.tls ? F("on") : F("off")) + F("\",\"jh\":\"") + jeedom.hostname + F("\",\"ja\":\"") + (__FlashStringHelper*)appDataPredefPassword + F("\",\"ci\":") + jeedom.cmdId + F(",\"jfp\":\"") + Utils::FingerPrintA2S(fpStr, jeedom.fingerPrint, ':') + '"';

  return gc;
}

//Parse HTTP Request into an AppData1 structure
bool AppData1::SetFromParameters(AsyncWebServerRequest* request, AppData1 &tempAppData1) {

  if (request->hasParam(F("inv"), true)) tempAppData1.invert = (request->getParam(F("inv"), true)->value() == F("on"));

  if (request->hasParam(F("je"), true)) tempAppData1.jeedom.enabled = (request->getParam(F("je"), true)->value() == F("on"));
  if (request->hasParam(F("jt"), true)) tempAppData1.jeedom.tls = (request->getParam(F("jt"), true)->value() == F("on"));
  if (request->hasParam(F("jh"), true) && request->getParam(F("jh"), true)->value().length() < sizeof(tempAppData1.jeedom.hostname)) strcpy(tempAppData1.jeedom.hostname, request->getParam(F("jh"), true)->value().c_str());
  if (request->hasParam(F("ja"), true) && request->getParam(F("ja"), true)->value().length() < sizeof(tempAppData1.jeedom.apiKey)) strcpy(tempAppData1.jeedom.apiKey, request->getParam(F("ja"), true)->value().c_str());
  if (request->hasParam(F("ci"), true)) tempAppData1.jeedom.cmdId = request->getParam(F("ci"), true)->value().toInt();
  if (!tempAppData1.jeedom.hostname[0] || !tempAppData1.jeedom.apiKey[0] || !tempAppData1.jeedom.cmdId) tempAppData1.jeedom.enabled = false;
  if (request->hasParam(F("jfp"), true)) Utils::FingerPrintS2A(tempAppData1.jeedom.fingerPrint, request->getParam(F("jfp"), true)->value().c_str());

  //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
  if (!strcmp_P(tempAppData1.jeedom.apiKey, appDataPredefPassword)) strcpy(tempAppData1.jeedom.apiKey, jeedom.apiKey);

  return true;
}




void WebInput::TimerTick() {

  bool needSend = false;

  //if input pin is RX
#if SIGNAL_PIN == 3
  //flush then stop serial port
  Serial.flush();
  delay(5);
  Serial.end();
#endif

  //if input changed
  if ((digitalRead(SIGNAL_PIN)^(_appData1->invert)) != _state) {
    _state = (digitalRead(SIGNAL_PIN)^(_appData1->invert));
    needSend = true;
  }

  //if Jeedom upload not enabled then return
  if (!_appData1->jeedom.enabled) return;

  //if input pin is RX
#if SIGNAL_PIN == 3
  //restart Serial
  Serial.begin(SERIAL_SPEED);
#endif

  //send if input changed or last HTTP result is wrong
  if (needSend || _jeedomRequestResult != 200) {

    Serial.print(F("Send State : ")); Serial.println(_state ? 1 : 0);

    //Build request
    String _request = String(F("&type=virtual&id=")) + _appData1->jeedom.cmdId + F("&value=") + (_state ? 1 : 0);

    String completeURI;
    completeURI = completeURI + F("http") + (_appData1->jeedom.tls ? F("s") : F("")) + F("://") + _appData1->jeedom.hostname + F("/core/api/jeeApi.php?apikey=") + _appData1->jeedom.apiKey + _request;

    //create HTTP request
    HTTPClient http;

    //if tls is enabled or not, we need to provide certificate fingerPrint
    if (!_appData1->jeedom.tls) http.begin(completeURI);
    else {
      char fpStr[41];
      http.begin(completeURI, Utils::FingerPrintA2S(fpStr, _appData1->jeedom.fingerPrint));
    }

    _jeedomRequestResult = http.GET();
    http.end();
  }
}


//------------------------------------------
//return WebInput Status in JSON
String WebInput::GetStatus() {

  String statusJSON('{');
  statusJSON = statusJSON + F("\"input\":") + (_state ? '1' : '0') ;
  statusJSON = statusJSON + F(",\"ljr\":") + _jeedomRequestResult;
  statusJSON += '}';

  return statusJSON;
}


//------------------------------------------
//Function to initiate WebInput with Config
void WebInput::Init(AppData1 &appData1) {

  Serial.print(F("Start WebInput"));

  _appData1 = &appData1;

  //If a Pin is defined to control signal arrival
#ifdef SIGNAL_CONTROL_PIN
  pinMode(SIGNAL_CONTROL_PIN, OUTPUT);
  digitalWrite(SIGNAL_CONTROL_PIN, LOW);
#endif

  //Setup SimpleTimer object to run code every X millisecs
  _refreshTimer.setInterval(1000, [this]() {
    this->TimerTick();
  });

  Serial.println(F(" : OK"));
}

//------------------------------------------
void WebInput::InitWebServer(AsyncWebServer &server) {

  server.on("/gs1", HTTP_GET, [this](AsyncWebServerRequest * request) {
    request->send(200, F("text/json"), GetStatus());
  });
}

//------------------------------------------
//Run for timer
void WebInput::Run() {

  _refreshTimer.run();
}
