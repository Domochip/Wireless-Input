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

  //if Jeedom upload not enabled then return
  if (!jeedom.enabled)
    return;

    //if input pin is RX
#if SIGNAL_PIN == 3
  //restart Serial
  Serial.begin(SERIAL_SPEED);
#endif

  //send if input changed or last HTTP result is wrong
  if (needSend || _jeedomRequestResult != 200)
  {

    Serial.print(F("Send State : "));
    Serial.println(_state ? 1 : 0);

    //Build request
    String _request = String(F("&type=virtual&id=")) + jeedom.cmdId + F("&value=") + (_state ? 1 : 0);

    String completeURI;
    completeURI = completeURI + F("http") + (jeedom.tls ? F("s") : F("")) + F("://") + jeedom.hostname + F("/core/api/jeeApi.php?apikey=") + jeedom.apiKey + _request;

    //create HTTP request
    HTTPClient http;

    //if tls is enabled or not, we need to provide certificate fingerPrint
    if (!jeedom.tls)
      http.begin(completeURI);
    else
    {
      char fpStr[41];
      http.begin(completeURI, Utils::FingerPrintA2S(fpStr, jeedom.fingerPrint));
    }

    _jeedomRequestResult = http.GET();
    http.end();
  }
}

//------------------------------------------
//Used to initialize configuration properties to default values
void WebInput::SetConfigDefaultValues()
{
  invert = false;

  jeedom.enabled = false;
  jeedom.tls = true;
  jeedom.hostname[0] = 0;
  jeedom.apiKey[0] = 0;
  jeedom.cmdId = 0;
  memset(jeedom.fingerPrint, 0, 20);
};
//------------------------------------------
//Parse JSON object into configuration properties
void WebInput::ParseConfigJSON(JsonObject &root)
{
  if (root["inv"].success())
    invert = root["inv"];
  if (root["je"].success())
    jeedom.enabled = root["je"];
  if (root["jt"].success())
    jeedom.tls = root["jt"];
  if (root["jh"].success())
    strlcpy(jeedom.hostname, root["jh"], sizeof(jeedom.hostname));
  if (root["ja"].success())
    strlcpy(jeedom.apiKey, root["ja"], sizeof(jeedom.apiKey));
  if (root["ci"].success())
    jeedom.cmdId = root["ci"];
  if (root["jfp"].success())
    Utils::FingerPrintS2A(jeedom.fingerPrint, root["jfp"]);
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

  if (request->hasParam(F("je"), true))
    jeedom.enabled = (request->getParam(F("je"), true)->value() == F("on"));
  else
    jeedom.enabled = false;
  if (request->hasParam(F("jt"), true))
    jeedom.tls = (request->getParam(F("jt"), true)->value() == F("on"));
  else
    jeedom.tls = false;
  if (request->hasParam(F("jh"), true) && request->getParam(F("jh"), true)->value().length() < sizeof(jeedom.hostname))
    strcpy(jeedom.hostname, request->getParam(F("jh"), true)->value().c_str());
  //put apiKey into temporary one for predefpassword
  if (request->hasParam(F("ja"), true) && request->getParam(F("ja"), true)->value().length() < sizeof(tempApiKey))
    strcpy(tempApiKey, request->getParam(F("ja"), true)->value().c_str());
  if (request->hasParam(F("ci"), true))
    jeedom.cmdId = request->getParam(F("ci"), true)->value().toInt();
  if (request->hasParam(F("jfp"), true))
    Utils::FingerPrintS2A(jeedom.fingerPrint, request->getParam(F("jfp"), true)->value().c_str());

  //check for previous apiKey (there is a predefined special password that mean to keep already saved one)
  if (strcmp_P(tempApiKey, appDataPredefPassword))
    strcpy(jeedom.apiKey, tempApiKey);

  if (!jeedom.hostname[0] || !jeedom.apiKey[0] || !jeedom.cmdId)
    jeedom.enabled = false;

  return true;
};
//------------------------------------------
//Generate JSON from configuration properties
String WebInput::GenerateConfigJSON(bool forSaveFile = false)
{
  char fpStr[60];

  String gc('{');

  gc = gc + F("\"inv\":") + (invert ? true : false);

  gc = gc + F(",\"je\":") + (jeedom.enabled ? true : false);
  gc = gc + F(",\"jt\":") + (jeedom.tls ? true : false);
  gc = gc + F(",\"jh\":\"") + jeedom.hostname + '"';
  if (forSaveFile)
  {
    gc = gc + F(",\"ja\":\"") + jeedom.apiKey + '"';
    Utils::FingerPrintA2S(fpStr, jeedom.fingerPrint);
  }
  else
  {
    //there is a predefined special password (mean to keep already saved one)
    gc = gc + F(",\"ja\":\"") + (__FlashStringHelper *)appDataPredefPassword + '"';
    Utils::FingerPrintA2S(fpStr, jeedom.fingerPrint, ':');
  }
  gc = gc + F(",\"ci\":") + jeedom.cmdId;
  gc = gc + F(",\"jfp\":\"") + fpStr + '"';

  gc += '}';

  return gc;
};
//------------------------------------------
//Generate JSON of application status
String WebInput::GenerateStatusJSON()
{
  String gs('{');

  gs = gs + F("\"input\":") + _state;
  gs = gs + F(",\"ljr\":") + _jeedomRequestResult;

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