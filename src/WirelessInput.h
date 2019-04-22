#ifndef WirelessInput_h
#define WirelessInput_h

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "Main.h"
#include "base\Utils.h"
#include "base\Application.h"

const char appDataPredefPassword[] PROGMEM = "ewcXoCt4HHjZUvY1";

#include "data\status1.html.gz.h"
#include "data\config1.html.gz.h"

#include <ESP8266HTTPClient.h>
#include <PubSubClient.h>
#include <Ticker.h>

class WebInput : public Application
{
private:
#define HA_HTTP_GENERIC 0
#define HA_HTTP_JEEDOM_VIRTUAL 1

  typedef struct
  {
    byte type = HA_HTTP_GENERIC;
    int cmdId = 0;
    struct
    {
      char uriPattern[150 + 1] = {0};
    } generic;
    struct
    {
      char apiKey[48 + 1] = {0};
    } jeedom;
  } HTTP;

#define HA_MQTT_GENERIC 0

  typedef struct
  {
    byte type = HA_MQTT_GENERIC;
    uint32_t port = 1883;
    char username[128 + 1] = {0};
    char password[150 + 1] = {0};
    struct
    {
      char baseTopic[64 + 1] = {0};
    } generic;
  } MQTT;

#define HA_PROTO_DISABLED 0
#define HA_PROTO_HTTP 1
#define HA_PROTO_MQTT 2

  typedef struct
  {
    byte protocol = HA_PROTO_DISABLED;
    char hostname[64 + 1] = {0};
    bool tls = false;
    byte fingerPrint[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    HTTP http;
    MQTT mqtt;
  } HomeAutomation;

  bool _state = false;
  bool _invert = false;

  HomeAutomation _ha;
  int _haSendResult = 0;

  bool _needRead = false;
  Ticker _readTicker;
  WiFiClient _wifiMqttClient;
  WiFiClientSecure _wifiMqttClientSecure;
  PubSubClient _mqttClient;
  bool _needMqttReconnect = false;
  Ticker _mqttReconnectTicker;

  void ReadTick();
  bool MqttConnect();
  void MqttCallback(char *topic, uint8_t *payload, unsigned int length);

  void SetConfigDefaultValues();
  void ParseConfigJSON(DynamicJsonDocument &doc);
  bool ParseConfigWebRequest(AsyncWebServerRequest *request);
  String GenerateConfigJSON(bool forSaveFile);
  String GenerateStatusJSON();
  bool AppInit(bool reInit);
  const uint8_t *GetHTMLContent(WebPageForPlaceHolder wp);
  size_t GetHTMLContentSize(WebPageForPlaceHolder wp);
  void AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication);
  void AppRun();

public:
  WebInput(char appId, String fileName);
};

#endif
