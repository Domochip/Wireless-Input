#include "WirelessInput.h"

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

  //if Home Automation upload not enabled then return
  if (!ha.enabled)
    return;

    //if input pin is RX
#if SIGNAL_PIN == 3
  //restart Serial
  Serial.begin(SERIAL_SPEED);
#endif

  //send if input changed or last HTTP result is wrong
  if (needSend || _haRequestResult != 200)
  {

    Serial.print(F("Send State : "));
    Serial.println(_state ? 1 : 0);

    String completeURI;

    //Build request
    switch (ha.enabled)
    {
    case 1: //jeedom
      String _request = String(F("&type=virtual&id=")) + ha.cmdId + F("&value=") + (_state ? 1 : 0);
      completeURI = completeURI + F("http") + (ha.tls ? F("s") : F("")) + F("://") + ha.hostname + F("/core/api/jeeApi.php?apikey=") + ha.jeedom.apiKey + _request;
      break;
    }

    //create HTTP request
    HTTPClient http;

    //if tls is enabled or not, we need to provide certificate fingerPrint
    if (!ha.tls)
      http.begin(completeURI);
    else
    {
      char fpStr[41];
      http.begin(completeURI, Utils::FingerPrintA2S(fpStr, ha.fingerPrint));
    }

    _haRequestResult = http.GET();
    http.end();
  }
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebInput::SetConfigDefaultValues()
{
  invert = false;

  ha.enabled = 0;
  ha.tls = true;
  ha.hostname[0] = 0;
  ha.cmdId = 0;
  memset(ha.fingerPrint, 0, 20);

  ha.jeedom.apiKey[0] = 0;
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebInput::ParseConfigJSON(JsonObject &root)
{
  //Retrocompatibility block to be removed after v1.1.1 --
  if (root["je"].success())
    ha.enabled = root["je"] ? 1 : 0;
  if (root["jt"].success())
    ha.tls = root["jt"];
  if (root["jh"].success())
    strlcpy(ha.hostname, root["jh"], sizeof(ha.hostname));
  if (root["ci"].success())
    ha.cmdId = root["ci"];
  if (root["jfp"].success())
    Utils::FingerPrintS2A(ha.fingerPrint, root["jfp"]);
  // --

  if (root["inv"].success())
    invert = root["inv"];

  if (root[F("hae")].success())
    ha.enabled = root[F("hae")];
  if (root[F("hatls")].success())
    ha.tls = root[F("hatls")];
  if (root[F("hah")].success())
    strlcpy(ha.hostname, root["hah"], sizeof(ha.hostname));
  if (root["hacid"].success())
    ha.cmdId = root["hacid"];
  if (root["hafp"].success())
    Utils::FingerPrintS2A(ha.fingerPrint, root["hafp"]);

  if (root["ja"].success())
    strlcpy(ha.jeedom.apiKey, root["ja"], sizeof(ha.jeedom.apiKey));
};
//------------------------------------------
//Parse HTTP POST parameters in request into configuration properties
bool WebInput::ParseConfigWebRequest(AsyncWebServerRequest *request)
{

  char tempApiKey[48 + 1];

  if (request->hasParam(F("inv"), true))
    invert = (request->getParam(F("inv"), true)->value() == F("on"));
  else
    invert = false;

  if (request->hasParam(F("hae"), true))
    ha.enabled = request->getParam(F("hae"), true)->value().toInt();

  //if an home Automation system is enabled then get common param
  if (ha.enabled)
  {
    if (request->hasParam(F("hatls"), true))
      ha.tls = (request->getParam(F("hatls"), true)->value() == F("on"));
    else
      ha.tls = false;
    if (request->hasParam(F("hah"), true) && request->getParam(F("hah"), true)->value().length() < sizeof(ha.hostname))
      strcpy(ha.hostname, request->getParam(F("hah"), true)->value().c_str());
    if (request->hasParam(F("hacid"), true))
      ha.cmdId = request->getParam(F("hacid"), true)->value().toInt();
    if (request->hasParam(F("hafp"), true))
      Utils::FingerPrintS2A(ha.fingerPrint, request->getParam(F("hafp"), true)->value().c_str());
  }

  //Now get specific param
  switch (ha.enabled)
  {
  case 1: //Jeedom
    char tempApiKey[48 + 1];
    //put apiKey into temporary one for predefpassword
    if (request->hasParam(F("ja"), true) && request->getParam(F("ja"), true)->value().length() < sizeof(tempApiKey))
      strcpy(tempApiKey, request->getParam(F("ja"), true)->value().c_str());
    //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
    if (strcmp_P(tempApiKey, appDataPredefPassword))
      strcpy(ha.jeedom.apiKey, tempApiKey);
    if (!ha.hostname[0] || !ha.jeedom.apiKey[0] || !ha.cmdId)
      ha.enabled = 0;
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

  gc = gc + F("\"inv\":") + (invert ? true : false);

  gc = gc + F(",\"hae\":") + ha.enabled;
  gc = gc + F(",\"hatls\":") + ha.tls;
  gc = gc + F(",\"hah\":\"") + ha.hostname + '"';
  gc = gc + F(",\"hacid\":") + ha.cmdId;
  gc = gc + F(",\"hafp\":\"") + Utils::FingerPrintA2S(fpStr, ha.fingerPrint, forSaveFile ? 0 : ':') + '"';

  if (forSaveFile)
  {
    if (ha.enabled == 1)
      gc = gc + F(",\"ja\":\"") + ha.jeedom.apiKey + '"';
  }
  else
    gc = gc + F(",\"ja\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"'; //predefined special password (mean to keep already saved one)

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebInput::GenerateStatusJSON()
{
  String gs('{');

  gs = gs + F("\"input\":") + _state;
  gs = gs + F(",\"lhar\":") + _haRequestResult;

  gs += '}';

  return gs;
};
//------------------------------------------
//code to execute during initialization and reinitialization of the app
bool WebInput::AppInit(bool reInit)
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

  return true;
};
//------------------------------------------
//code to register web request answer to the web server
void WebInput::AppInitWebServer(AsyncWebServer &server, bool &shouldReboot, bool &pauseApplication){
    //Nothing to do
};

//------------------------------------------
//Run for timer
void WebInput::AppRun()
{
  _refreshTimer.run();
}

//------------------------------------------
//Constructor
WebInput::WebInput(char appId, String appName) : Application(appId, appName) {}