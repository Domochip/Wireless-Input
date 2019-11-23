#include "WirelessInput.h"

//------------------------------------------
// Connect then Subscribe to MQTT
void WebInput::mqttConnectedCallback(MQTTMan *mqttMan, bool firstConnection) {}

//------------------------------------------
//Callback used when an MQTT message arrived
void WebInput::mqttCallback(char *topic, uint8_t *payload, unsigned int length) {}

//------------------------------------------
// Execute code to read and publish input state
void WebInput::readTick()
{
  bool needSend = false;

  //if input pin is RX
#if SIGNAL_PIN == 3
  //flush then stop serial port
  LOG_SERIAL.flush();
  delay(5);
  LOG_SERIAL.end();
#endif

  //if input changed
  if ((digitalRead(SIGNAL_PIN) ^ _invert) != _state)
  {
    _state = (digitalRead(SIGNAL_PIN) ^ _invert);
    needSend = true;
  }

  //if input pin is RX
#if SIGNAL_PIN == 3
  //restart Serial
  LOG_SERIAL.begin(LOG_SERIAL_SPEED);
#endif

  //if Home Automation upload not enabled then return
  if (_ha.protocol == HA_PROTO_DISABLED)
    return;

  //send if input changed or last send result is wrong
  if (needSend || !_haSendResult)
  {

    LOG_SERIAL.print(F("Send State : "));
    LOG_SERIAL.println(_state ? 1 : 0);

    //----- HTTP Protocol configured -----
    if (_ha.protocol == HA_PROTO_HTTP)
    {
      String completeURI;

      //Build request
      switch (_ha.http.type)
      {
      case HA_HTTP_GENERIC:
        completeURI = _ha.http.generic.uriPattern;
        break;
      case HA_HTTP_JEEDOM_VIRTUAL:
        completeURI = F("http$tls$://$host$/core/api/jeeApi.php?apikey=$apikey$&type=virtual&id=$id$&value=$val$");
        break;
      }

      //Replace placeholders
      if (completeURI.indexOf(F("$tls$")) != -1)
        completeURI.replace(F("$tls$"), _ha.http.tls ? "s" : "");

      if (completeURI.indexOf(F("$host$")) != -1)
        completeURI.replace(F("$host$"), _ha.hostname);

      if (completeURI.indexOf(F("$id$")) != -1)
        completeURI.replace(F("$id$"), String(_ha.http.cmdId));

      if (completeURI.indexOf(F("$val$")) != -1)
        completeURI.replace(F("$val$"), (_state ? "1" : "0"));

      if (completeURI.indexOf(F("$apikey$")) != -1)
        completeURI.replace(F("$apikey$"), _ha.http.jeedom.apiKey);

      //create HTTP request
      HTTPClient http;

      //if tls is enabled or not, we need to provide certificate fingerPrint
      if (!_ha.http.tls)
        http.begin(_wifiClient, completeURI);
      else
      {
        if (Utils::isFingerPrintEmpty(_ha.http.fingerPrint))
          _wifiClientSecure.setInsecure();
        else
          _wifiClientSecure.setFingerprint(_ha.http.fingerPrint);
        http.begin(_wifiClientSecure, completeURI);
      }

      _haSendResult = (http.GET() == 200);
      http.end();
    }

    //----- MQTT Protocol configured -----
    if (_ha.protocol == HA_PROTO_MQTT)
    {
      //if we are connected
      if (_mqttMan.connected())
      {
        //prepare topic
        String completeTopic = _ha.mqtt.generic.baseTopic;

        //Replace placeholders
        MQTTMan::prepareTopic(completeTopic);

        switch (_ha.mqtt.type)
        {
        case HA_MQTT_GENERIC:
          //complete the topic
          completeTopic += F("status");
          break;
        }

        //send
        _haSendResult = _mqttMan.publish(completeTopic.c_str(), _state ? "1" : "0");
      }
    }
  }
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebInput::setConfigDefaultValues()
{
  _invert = false;

  _ha.protocol = HA_PROTO_DISABLED;
  _ha.hostname[0] = 0;

  _ha.http.type = HA_HTTP_GENERIC;
  _ha.http.tls = true;
  memset(_ha.http.fingerPrint, 0, 20);
  _ha.http.cmdId = 0;
  _ha.http.generic.uriPattern[0] = 0;
  _ha.http.jeedom.apiKey[0] = 0;

  _ha.mqtt.type = HA_MQTT_GENERIC;
  _ha.mqtt.port = 1883;
  _ha.mqtt.username[0] = 0;
  _ha.mqtt.password[0] = 0;
  _ha.mqtt.generic.baseTopic[0] = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebInput::parseConfigJSON(DynamicJsonDocument &doc)
{
  if (!doc[F("inv")].isNull())
    _invert = doc[F("inv")];

  if (!doc[F("haproto")].isNull())
    _ha.protocol = doc[F("haproto")];
  if (!doc[F("hahost")].isNull())
    strlcpy(_ha.hostname, doc[F("hahost")], sizeof(_ha.hostname));

  if (!doc[F("hahtype")].isNull())
    _ha.http.type = doc[F("hahtype")];
  if (!doc[F("hahtls")].isNull())
    _ha.http.tls = doc[F("hahtls")];
  if (!doc[F("hahfp")].isNull())
    Utils::fingerPrintS2A(_ha.http.fingerPrint, doc[F("hahfp")]);
  if (!doc[F("hahcid")].isNull())
    _ha.http.cmdId = doc[F("hahcid")];

  if (!doc[F("hahgup")].isNull())
    strlcpy(_ha.http.generic.uriPattern, doc[F("hahgup")], sizeof(_ha.http.generic.uriPattern));

  if (!doc[F("hahjak")].isNull())
    strlcpy(_ha.http.jeedom.apiKey, doc[F("hahjak")], sizeof(_ha.http.jeedom.apiKey));

  if (!doc[F("hamtype")].isNull())
    _ha.mqtt.type = doc[F("hamtype")];
  if (!doc[F("hamport")].isNull())
    _ha.mqtt.port = doc[F("hamport")];
  if (!doc[F("hamu")].isNull())
    strlcpy(_ha.mqtt.username, doc[F("hamu")], sizeof(_ha.mqtt.username));
  if (!doc[F("hamp")].isNull())
    strlcpy(_ha.mqtt.password, doc[F("hamp")], sizeof(_ha.mqtt.password));

  if (!doc[F("hamgbt")].isNull())
    strlcpy(_ha.mqtt.generic.baseTopic, doc[F("hamgbt")], sizeof(_ha.mqtt.generic.baseTopic));
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebInput::parseConfigWebRequest(AsyncWebServerRequest *request)
{
  if (request->hasParam(F("inv"), true))
    _invert = (request->getParam(F("inv"), true)->value() == F("on"));
  else
    _invert = false;

  //Parse HA protocol
  if (request->hasParam(F("haproto"), true))
    _ha.protocol = request->getParam(F("haproto"), true)->value().toInt();

  //if an home Automation protocol has been selected then get common param
  if (_ha.protocol != HA_PROTO_DISABLED)
  {
    if (request->hasParam(F("hahost"), true) && request->getParam(F("hahost"), true)->value().length() < sizeof(_ha.hostname))
      strcpy(_ha.hostname, request->getParam(F("hahost"), true)->value().c_str());
  }

  //Now get specific param
  switch (_ha.protocol)
  {
  case HA_PROTO_HTTP:

    if (request->hasParam(F("hahtype"), true))
      _ha.http.type = request->getParam(F("hahtype"), true)->value().toInt();
    if (request->hasParam(F("hahtls"), true))
      _ha.http.tls = (request->getParam(F("hahtls"), true)->value() == F("on"));
    else
      _ha.http.tls = false;
    if (request->hasParam(F("hahfp"), true))
      Utils::fingerPrintS2A(_ha.http.fingerPrint, request->getParam(F("hahfp"), true)->value().c_str());
    if (request->hasParam(F("hahcid"), true))
      _ha.http.cmdId = request->getParam(F("hahcid"), true)->value().toInt();

    switch (_ha.http.type)
    {
    case HA_HTTP_GENERIC:
      if (request->hasParam(F("hahgup"), true) && request->getParam(F("hahgup"), true)->value().length() < sizeof(_ha.http.generic.uriPattern))
        strcpy(_ha.http.generic.uriPattern, request->getParam(F("hahgup"), true)->value().c_str());
      if (!_ha.hostname[0] || !_ha.http.cmdId || !_ha.http.generic.uriPattern[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    case HA_HTTP_JEEDOM_VIRTUAL:
      char tempApiKey[48 + 1];
      //put apiKey into temporary one for predefpassword
      if (request->hasParam(F("hahjak"), true) && request->getParam(F("hahjak"), true)->value().length() < sizeof(tempApiKey))
        strcpy(tempApiKey, request->getParam(F("hahjak"), true)->value().c_str());
      //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
      if (strcmp_P(tempApiKey, appDataPredefPassword))
        strcpy(_ha.http.jeedom.apiKey, tempApiKey);
      if (!_ha.hostname[0] || !_ha.http.cmdId || !_ha.http.jeedom.apiKey[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;

  case HA_PROTO_MQTT:

    if (request->hasParam(F("hamtype"), true))
      _ha.mqtt.type = request->getParam(F("hamtype"), true)->value().toInt();
    if (request->hasParam(F("hamport"), true))
      _ha.mqtt.port = request->getParam(F("hamport"), true)->value().toInt();
    if (request->hasParam(F("hamu"), true) && request->getParam(F("hamu"), true)->value().length() < sizeof(_ha.mqtt.username))
      strcpy(_ha.mqtt.username, request->getParam(F("hamu"), true)->value().c_str());
    char tempPassword[64 + 1] = {0};
    //put MQTT password into temporary one for predefpassword
    if (request->hasParam(F("hamp"), true) && request->getParam(F("hamp"), true)->value().length() < sizeof(tempPassword))
      strcpy(tempPassword, request->getParam(F("hamp"), true)->value().c_str());
    //check for previous password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempPassword, appDataPredefPassword))
      strcpy(_ha.mqtt.password, tempPassword);

    switch (_ha.mqtt.type)
    {
    case HA_MQTT_GENERIC:
      if (request->hasParam(F("hamgbt"), true) && request->getParam(F("hamgbt"), true)->value().length() < sizeof(_ha.mqtt.generic.baseTopic))
        strcpy(_ha.mqtt.generic.baseTopic, request->getParam(F("hamgbt"), true)->value().c_str());

      if (!_ha.hostname[0] || !_ha.mqtt.generic.baseTopic[0])
        _ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;
  }

  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebInput::generateConfigJSON(bool forSaveFile = false)
{
  char fpStr[60];

  String gc('{');

  gc = gc + F("\"inv\":") + _invert;

  gc = gc + F(",\"haproto\":") + _ha.protocol;
  gc = gc + F(",\"hahost\":\"") + _ha.hostname + '"';

  //if for WebPage or protocol selected is HTTP
  if (!forSaveFile || _ha.protocol == HA_PROTO_HTTP)
  {
    gc = gc + F(",\"hahtype\":") + _ha.http.type;
    gc = gc + F(",\"hahtls\":") + _ha.http.tls;
    gc = gc + F(",\"hahfp\":\"") + Utils::fingerPrintA2S(fpStr, _ha.http.fingerPrint, forSaveFile ? 0 : ':') + '"';
    gc = gc + F(",\"hahcid\":") + _ha.http.cmdId;
    gc = gc + F(",\"hahgup\":\"") + _ha.http.generic.uriPattern + '"';

    if (forSaveFile)
      gc = gc + F(",\"hahjak\":\"") + _ha.http.jeedom.apiKey + '"';
    else
      gc = gc + F(",\"hahjak\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)
  }

  //if for WebPage or protocol selected is MQTT
  if (!forSaveFile || _ha.protocol == HA_PROTO_MQTT)
  {
    gc = gc + F(",\"hamtype\":") + _ha.mqtt.type;
    gc = gc + F(",\"hamport\":") + _ha.mqtt.port;
    gc = gc + F(",\"hamu\":\"") + _ha.mqtt.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hamp\":\"") + _ha.mqtt.password + '"';
    else
      gc = gc + F(",\"hamp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)

    gc = gc + F(",\"hamgbt\":\"") + _ha.mqtt.generic.baseTopic + '"';
  }

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebInput::generateStatusJSON()
{
  String gs('{');

  gs = gs + F("\"input\":") + _state;

  gs = gs + F(",\"has1\":\"");
  switch (_ha.protocol)
  {
  case HA_PROTO_DISABLED:
    gs = gs + F("Disabled");
    break;
  case HA_PROTO_HTTP:
    gs = gs + F("Last HTTP request : ") + (_haSendResult ? F("OK") : F("Failed"));
    break;
  case HA_PROTO_MQTT:
    gs = gs + F("MQTT Connection State : ");
    switch (_mqttMan.state())
    {
    case MQTT_CONNECTION_TIMEOUT:
      gs = gs + F("Timed Out");
      break;
    case MQTT_CONNECTION_LOST:
      gs = gs + F("Lost");
      break;
    case MQTT_CONNECT_FAILED:
      gs = gs + F("Failed");
      break;
    case MQTT_CONNECTED:
      gs = gs + F("Connected");
      break;
    case MQTT_CONNECT_BAD_PROTOCOL:
      gs = gs + F("Bad Protocol Version");
      break;
    case MQTT_CONNECT_BAD_CLIENT_ID:
      gs = gs + F("Incorrect ClientID ");
      break;
    case MQTT_CONNECT_UNAVAILABLE:
      gs = gs + F("Server Unavailable");
      break;
    case MQTT_CONNECT_BAD_CREDENTIALS:
      gs = gs + F("Bad Credentials");
      break;
    case MQTT_CONNECT_UNAUTHORIZED:
      gs = gs + F("Connection Unauthorized");
      break;
    }

    if (_mqttMan.state() == MQTT_CONNECTED)
      gs = gs + F("\",\"has2\":\"Last Publish Result : ") + (_haSendResult ? F("OK") : F("Failed"));

    break;
  }
  gs += '"';

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebInput::appInit(bool reInit)
{
  //Stop Read input
  _readTicker.detach();

  //Stop MQTT
  _mqttMan.disconnect();

  //if MQTT used so configure it
  if (_ha.protocol == HA_PROTO_MQTT)
  {
    //prepare will topic
    String willTopic = _ha.mqtt.generic.baseTopic;
    MQTTMan::prepareTopic(willTopic);
    willTopic += F("connected");

    //setup MQTT
    _mqttMan.setClient(_wifiClient).setServer(_ha.hostname, _ha.mqtt.port);
    _mqttMan.setConnectedAndWillTopic(willTopic.c_str());
    _mqttMan.setConnectedCallback(std::bind(&WebInput::mqttConnectedCallback, this, std::placeholders::_1, std::placeholders::_2));
    _mqttMan.setCallback(std::bind(&WebInput::mqttCallback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

    //Connect
    _mqttMan.connect(_ha.mqtt.username, _ha.mqtt.password);
  }

//If a Pin is defined to control signal arrival
#ifdef SIGNAL_CONTROL_PIN
  pinMode(SIGNAL_CONTROL_PIN, OUTPUT);
  digitalWrite(SIGNAL_CONTROL_PIN, LOW);
#endif

  _readTicker.attach(1, [this]() { this->_needRead = true; });

  return true;
};
//------------------------------------------
//Return HTML Code to insert into Status Web page
const uint8_t *WebInput::getHTMLContent(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return (const uint8_t *)status1htmlgz;
    break;
  case config:
    return (const uint8_t *)config1htmlgz;
    break;
  default:
    return nullptr;
    break;
  };
  return nullptr;
};
//and his Size
size_t WebInput::getHTMLContentSize(WebPageForPlaceHolder wp)
{
  switch (wp)
  {
  case status:
    return sizeof(status1htmlgz);
    break;
  case config:
    return sizeof(config1htmlgz);
    break;
  default:
    return 0;
    break;
  };
  return 0;
};
//------------------------------------------
//code to register web request answer to the web server
void WebInput::appInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication){
    //Nothing to do
};

//------------------------------------------
//Run for timer and MQTT if required
void WebInput::appRun()
{
  if (_ha.protocol == HA_PROTO_MQTT)
    _mqttMan.loop();

  if (_needRead)
  {
    _needRead = false;
    LOG_SERIAL.println(F("ReadTick"));
    readTick();
  }
}

//------------------------------------------
//Constructor
WebInput::WebInput(char appId, String appName) : Application(appId, appName) {}