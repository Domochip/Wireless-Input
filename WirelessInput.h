#ifndef WirelessInput_h
#define WirelessInput_h

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Main.h"
#include "src\Utils.h"
#include "src\Base.h"

#include "SimpleTimer.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

class WebInput : public Application
{
private:
  bool invert = false;

  typedef struct
  {
    char apiKey[48 + 1] = {0};
  } Jeedom;

  typedef struct
  {
    byte enabled = 0; //0 : no HA; 1 : Jeedom; 2 : ...
    bool tls = false;
    byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    char hostname[64 + 1] = {0};
    int cmdId = 0;
    Jeedom jeedom;
  } HomeAutomation;
  HomeAutomation ha;

  bool _state = false;
  SimpleTimer _refreshTimer;
  int _haRequestResult = 0;

  void TimerTick();

  void SetConfigDefaultValues();
  void ParseConfigJSON(JsonObject &root);
  bool ParseConfigWebRequest(AsyncWebServerRequest *request);
  String GenerateConfigJSON(bool forSaveFile);
  String GenerateStatusJSON();
  bool AppInit(bool reInit);
  void AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void AppRun();

public:
  WebInput(char appId, String fileName);
};

#endif
