#include "WirelessInput.h"

//Please, have a look at Main.h for information and configuration of Arduino project

void WebInput::TimerTick()
{

  bool needSend = false;

  //if input pin is RX
#if SIGNAL_PIN == 3
  //flush then stop serial port
  Serial.flush();
  delay(5);
  Serial.end();
#endif

  //if input changed
  if ((digitalRead(SIGNAL_PIN) ^ invert) != _state)
  {
    _state = (digitalRead(SIGNAL_PIN) ^ invert);
    needSend = true;
  }

    //if input pin is RX
#if SIGNAL_PIN == 3
  //restart Serial
  Serial.begin(SERIAL_SPEED);
#endif

  //if Home Automation upload not enabled then return
  if (ha.protocol == HA_PROTO_DISABLED)
    return;

  //send if input changed or last send result is wrong
  if (needSend || _haSendResult < 1)
  {

    Serial.print(F("Send State : "));
    Serial.println(_state ? 1 : 0);

    //----- HTTP Protocol configured -----
    if (ha.protocol == HA_PROTO_HTTP)
    {
      String completeURI;

      //Build request
      switch (ha.http.type)
      {
      case HA_HTTP_GENERIC:
        completeURI = ha.http.generic.uriPattern;
        break;
      case HA_HTTP_JEEDOM_VIRTUAL:
        completeURI = F("http$tls$://$host$/core/api/jeeApi.php?apikey=$apikey$&type=virtual&id=$id$&value=$val$");
        break;
      }

      //Replace placeholders
      if (completeURI.indexOf(F("$tls$")) != -1)
        completeURI.replace(F("$tls$"), ha.tls ? "s" : "");

      if (completeURI.indexOf(F("$host$")) != -1)
        completeURI.replace(F("$host$"), ha.hostname);

      if (completeURI.indexOf(F("$id$")) != -1)
        completeURI.replace(F("$id$"), String(ha.http.cmdId));

      if (completeURI.indexOf(F("$val$")) != -1)
        completeURI.replace(F("$val$"), (_state ? "1" : "0"));

      if (completeURI.indexOf(F("$apikey$")) != -1)
        completeURI.replace(F("$apikey$"), ha.http.jeedom.apiKey);

      //create HTTP request
      HTTPClient http;

      //if tls is enabled or not, we need to provide certificate fingerPrint
      if (!ha.tls)
        http.begin(completeURI);
      else
      {
        char fpStr[41];
        http.begin(completeURI, Utils::FingerPrintA2S(fpStr, ha.http.fingerPrint));
      }

      _haSendResult = (http.GET() == 200);
      http.end();
    }

    //----- MQTT Protocol configured -----
    if (ha.protocol == HA_PROTO_MQTT)
    {
      //sn can be used in multiple cases
      char sn[9];
      sprintf_P(sn, PSTR("%08x"), ESP.getChipId());

      //if not connected to MQTT
      if (!_pubSubClient->connected())
      {
        //generate clientID
        String clientID(F(APPLICATION1_NAME));
        clientID += sn;
        //and try to connect
        if (!ha.mqtt.username[0])
          _pubSubClient->connect(clientID.c_str());
        else
        {
          if (!ha.mqtt.password[0])
            _pubSubClient->connect(clientID.c_str(), ha.mqtt.username, NULL);
          else
            _pubSubClient->connect(clientID.c_str(), ha.mqtt.username, ha.mqtt.password);
        }
      }

      //if still not connected
      if (!_pubSubClient->connected())
      {
        //return error code minus 10 (result should be negative)
        _haSendResult = _pubSubClient->state();
        _haSendResult -= 10;
      }
      // else we are connected
      else
      {
        //prepare topic
        String completeTopic;
        switch (ha.mqtt.type)
        {
        case HA_MQTT_GENERIC:
          completeTopic = ha.mqtt.generic.baseTopic;

          //check for final slash
          if (completeTopic.length() && completeTopic.charAt(completeTopic.length()-1) != '/')
            completeTopic += '/';
          //complete the topic
          completeTopic += F("status");
          break;
        }

        //Replace placeholders
        if (completeTopic.indexOf(F("$sn$")) != -1)
          completeTopic.replace(F("$sn$"), sn);

        if (completeTopic.indexOf(F("$mac$")) != -1)
          completeTopic.replace(F("$mac$"), WiFi.macAddress());

        if (completeTopic.indexOf(F("$model$")) != -1)
          completeTopic.replace(F("$model$"), APPLICATION1_NAME);

        //send
        _haSendResult = _pubSubClient->publish(completeTopic.c_str(), _state ? "1" : "0");
      }
    }
  }
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebInput::SetConfigDefaultValues()
{
  invert = false;

  ha.protocol = HA_PROTO_DISABLED;
  ha.tls = true;
  ha.hostname[0] = 0;

  ha.http.type = HA_HTTP_GENERIC;
  memset(ha.http.fingerPrint, 0, 20);
  ha.http.cmdId = 0;
  ha.http.generic.uriPattern[0] = 0;
  ha.http.jeedom.apiKey[0] = 0;

  ha.mqtt.type = HA_MQTT_GENERIC;
  ha.mqtt.port = 1883;
  ha.mqtt.username[0] = 0;
  ha.mqtt.password[0] = 0;
  ha.mqtt.generic.baseTopic[0] = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebInput::ParseConfigJSON(JsonObject &root)
{
  if (root[F("inv")].success())
    invert = root[F("inv")];

  if (root[F("haproto")].success())
    ha.protocol = root[F("haproto")];
  if (root[F("hatls")].success())
    ha.tls = root[F("hatls")];
  if (root[F("hahost")].success())
    strlcpy(ha.hostname, root[F("hahost")], sizeof(ha.hostname));

  if (root[F("hahtype")].success())
    ha.http.type = root[F("hahtype")];
  if (root[F("hahfp")].success())
    Utils::FingerPrintS2A(ha.http.fingerPrint, root[F("hahfp")]);
  if (root[F("hahcid")].success())
    ha.http.cmdId = root[F("hahcid")];

  if (root[F("hahgup")].success())
    strlcpy(ha.http.generic.uriPattern, root[F("hahgup")], sizeof(ha.http.generic.uriPattern));

  if (root[F("hahjak")].success())
    strlcpy(ha.http.jeedom.apiKey, root[F("hahjak")], sizeof(ha.http.jeedom.apiKey));

  if (root[F("hamtype")].success())
    ha.mqtt.type = root[F("hamtype")];
  if (root[F("hamport")].success())
    ha.mqtt.port = root[F("hamport")];
  if (root[F("hamu")].success())
    strlcpy(ha.mqtt.username, root[F("hamu")], sizeof(ha.mqtt.username));
  if (root[F("hamp")].success())
    strlcpy(ha.mqtt.password, root[F("hamp")], sizeof(ha.mqtt.password));

  if (root[F("hamgbt")].success())
    strlcpy(ha.mqtt.generic.baseTopic, root[F("hamgbt")], sizeof(ha.mqtt.generic.baseTopic));
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebInput::ParseConfigWebRequest(AsyncWebServerRequest *request)
{
  if (request->hasParam(F("inv"), true))
    invert = (request->getParam(F("inv"), true)->value() == F("on"));
  else
    invert = false;

  //Parse HA protocol
  if (request->hasParam(F("haproto"), true))
    ha.protocol = request->getParam(F("haproto"), true)->value().toInt();

  //if an home Automation protocol has been selected then get common param
  if (ha.protocol != HA_PROTO_DISABLED)
  {
    if (request->hasParam(F("hatls"), true))
      ha.tls = (request->getParam(F("hatls"), true)->value() == F("on"));
    else
      ha.tls = false;
    if (request->hasParam(F("hahost"), true) && request->getParam(F("hahost"), true)->value().length() < sizeof(ha.hostname))
      strcpy(ha.hostname, request->getParam(F("hahost"), true)->value().c_str());
  }

  //Now get specific param
  switch (ha.protocol)
  {
  case HA_PROTO_HTTP:

    if (request->hasParam(F("hahtype"), true))
      ha.http.type = request->getParam(F("hahtype"), true)->value().toInt();
    if (request->hasParam(F("hahfp"), true))
      Utils::FingerPrintS2A(ha.http.fingerPrint, request->getParam(F("hahfp"), true)->value().c_str());
    if (request->hasParam(F("hahcid"), true))
      ha.http.cmdId = request->getParam(F("hahcid"), true)->value().toInt();

    switch (ha.http.type)
    {
    case HA_HTTP_GENERIC:
      if (request->hasParam(F("hahgup"), true) && request->getParam(F("hahgup"), true)->value().length() < sizeof(ha.http.generic.uriPattern))
        strcpy(ha.http.generic.uriPattern, request->getParam(F("hahgup"), true)->value().c_str());
      if (!ha.hostname[0] || !ha.http.cmdId || !ha.http.generic.uriPattern[0])
        ha.protocol = HA_PROTO_DISABLED;
      break;
    case HA_HTTP_JEEDOM_VIRTUAL:
      char tempApiKey[48 + 1];
      //put apiKey into temporary one for predefpassword
      if (request->hasParam(F("hahjak"), true) && request->getParam(F("hahjak"), true)->value().length() < sizeof(tempApiKey))
        strcpy(tempApiKey, request->getParam(F("hahjak"), true)->value().c_str());
      //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
      if (strcmp_P(tempApiKey, appDataPredefPassword))
        strcpy(ha.http.jeedom.apiKey, tempApiKey);
      if (!ha.hostname[0] || !ha.http.cmdId || !ha.http.jeedom.apiKey[0])
        ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;

  case HA_PROTO_MQTT:

    if (request->hasParam(F("hamtype"), true))
      ha.mqtt.type = request->getParam(F("hamtype"), true)->value().toInt();
    if (request->hasParam(F("hamport"), true))
      ha.mqtt.port = request->getParam(F("hamport"), true)->value().toInt();
    if (request->hasParam(F("hamu"), true) && request->getParam(F("hamu"), true)->value().length() < sizeof(ha.mqtt.username))
      strcpy(ha.mqtt.username, request->getParam(F("hamu"), true)->value().c_str());
    char tempPassword[64 + 1] = {0};
    //put MQTT password into temporary one for predefpassword
    if (request->hasParam(F("hamp"), true) && request->getParam(F("hamp"), true)->value().length() < sizeof(tempPassword))
      strcpy(tempPassword, request->getParam(F("hamp"), true)->value().c_str());
    //check for previous password (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempPassword, appDataPredefPassword))
      strcpy(ha.mqtt.password, tempPassword);

    switch (ha.mqtt.type)
    {
    case HA_MQTT_GENERIC:
      if (request->hasParam(F("hamgbt"), true) && request->getParam(F("hamgbt"), true)->value().length() < sizeof(ha.mqtt.generic.baseTopic))
        strcpy(ha.mqtt.generic.baseTopic, request->getParam(F("hamgbt"), true)->value().c_str());

      if (!ha.hostname[0] || !ha.mqtt.generic.baseTopic[0])
        ha.protocol = HA_PROTO_DISABLED;
      break;
    }
    break;
  }

  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebInput::GenerateConfigJSON(bool forSaveFile = false)
{
  char fpStr[60];

  String gc('{');

  gc = gc + F("\"inv\":") + invert;

  gc = gc + F(",\"haproto\":") + ha.protocol;
  gc = gc + F(",\"hatls\":") + ha.tls;
  gc = gc + F(",\"hahost\":\"") + ha.hostname + '"';

  //if for WebPage or protocol selected is HTTP
  if (!forSaveFile || ha.protocol == HA_PROTO_HTTP)
  {
    gc = gc + F(",\"hahtype\":") + ha.http.type;
    gc = gc + F(",\"hahfp\":\"") + Utils::FingerPrintA2S(fpStr, ha.http.fingerPrint, forSaveFile ? 0 : ':') + '"';
    gc = gc + F(",\"hahcid\":") + ha.http.cmdId;

    gc = gc + F(",\"hahgup\":\"") + ha.http.generic.uriPattern + '"';

    if (forSaveFile)
      gc = gc + F(",\"hahjak\":\"") + ha.http.jeedom.apiKey + '"';
    else
      gc = gc + F(",\"hahjak\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)
  }

  //if for WebPage or protocol selected is MQTT
  if (!forSaveFile || ha.protocol == HA_PROTO_MQTT)
  {
    gc = gc + F(",\"hamtype\":") + ha.mqtt.type;
    gc = gc + F(",\"hamport\":") + ha.mqtt.port;
    gc = gc + F(",\"hamu\":\"") + ha.mqtt.username + '"';
    if (forSaveFile)
      gc = gc + F(",\"hamp\":\"") + ha.mqtt.password + '"';
    else
      gc = gc + F(",\"hamp\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)

    gc = gc + F(",\"hamgbt\":\"") + ha.mqtt.generic.baseTopic + '"';
  }

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebInput::GenerateStatusJSON()
{
  String gs('{');

  gs = gs + F("\"input\":") + _state;
  if (ha.protocol != HA_PROTO_DISABLED)
    gs = gs + F(",\"lhar\":") + _haSendResult;

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebInput::AppInit(bool reInit)
{

  //Clean up MQTT variables
  if (_pubSubClient)
  {
    if (_pubSubClient->connected())
      _pubSubClient->disconnect();
    delete _pubSubClient;
    _pubSubClient = NULL;
  }
  if (_wifiClient)
  {
    delete _wifiClient;
    _wifiClient = NULL;
  }
  if (_wifiClientSecure)
  {
    delete _wifiClientSecure;
    _wifiClientSecure = NULL;
  }

  //if MQTT used so build MQTT variables
  if (ha.protocol == HA_PROTO_MQTT)
  {

    if (!ha.tls)
    {
      _wifiClient = new WiFiClient();
      _pubSubClient = new PubSubClient(ha.hostname, ha.mqtt.port, *_wifiClient);
    }
    else
    {
      _wifiClientSecure = new WiFiClientSecure();
      _pubSubClient = new PubSubClient(ha.hostname, ha.mqtt.port, *_wifiClientSecure);
    }
  }

  if (!reInit)
  {
//If a Pin is defined to control signal arrival
#ifdef SIGNAL_CONTROL_PIN
    pinMode(SIGNAL_CONTROL_PIN, OUTPUT);
    digitalWrite(SIGNAL_CONTROL_PIN, LOW);
#endif

    //Setup SimpleTimer object to run code every X millisecs
    _refreshTimer.setInterval(1000, [this]() {
      this->TimerTick();
    });
  }
  return true;
};
//------------------------------------------
//Return HTML Code to insert into Status Web page
const uint8_t* WebInput::GetHTMLContent(WebPageForPlaceHolder wp){
      switch(wp){
    case status:
      return (const uint8_t*) status1htmlgz;
      break;
    case config:
      return (const uint8_t*) config1htmlgz;
      break;
    default:
      return nullptr;
      break;
  };
  return nullptr;
};
//and his Size
size_t WebInput::GetHTMLContentSize(WebPageForPlaceHolder wp){
  switch(wp){
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
void WebInput::AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication){
    //Nothing to do
};

//------------------------------------------
//Run for timer and MQTT if required
void WebInput::AppRun()
{
  if (_pubSubClient)
    _pubSubClient->loop();
  _refreshTimer.run();
}

//------------------------------------------
//Constructor
WebInput::WebInput(char appId, String appName) : Application(appId, appName) {}