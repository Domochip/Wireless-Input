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
    bool enabled = false;
    bool tls = false;
    char hostname[64 + 1] = {0};
    char apiKey[48 + 1] = {0};
    int cmdId = 0;
    byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  } Jeedom;
  Jeedom jeedom;

  bool _state = false;
  SimpleTimer _refreshTimer;
  int _jeedomRequestResult = 0;

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
