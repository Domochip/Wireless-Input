#ifndef WirelessInput_h
#define WirelessInput_h

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Main.h"
#include "src\Utils.h"

#include "SimpleTimer.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

//Structure of Application Data 1
class AppData1 {

  public:
    bool invert = false;

    typedef struct {
      bool enabled = false;
      bool tls = false;
      char hostname[64 + 1] = {0};
      char apiKey[48 + 1] = {0};
      int cmdId = 0;
      byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    } Jeedom;
    Jeedom jeedom;

    void SetDefaultValues() {
      invert = false;

      jeedom.enabled = false;
      jeedom.tls = true;
      jeedom.hostname[0] = 0;
      jeedom.apiKey[0] = 0;
      jeedom.cmdId = 0;
      memset(jeedom.fingerPrint, 0, 20);
    }

    String GetJSON();
    bool SetFromParameters(AsyncWebServerRequest* request, AppData1 &tempAppData);
};


class WebInput {

  private:
    AppData1* _appData1;

    bool _state = false;
    SimpleTimer _refreshTimer;
    int _jeedomRequestResult = 0;

    void TimerTick();

    String GetStatus();

  public:
    void Init(AppData1 &appData1);
    void InitWebServer(AsyncWebServer &server);
    void Run();
};

#endif
