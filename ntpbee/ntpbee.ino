#define IOTWEBCONF_DEBUG_DISABLED

#include <IotWebConf.h>
#include <ArduinoOTA.h>

// -- Initial name of the Thing. Used e.g. as SSID of the own Access Point.
const char thingName[] = "ntpbee";

// -- Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "timehascometoday";

const char configVersion[] = "ntpbee_v1";

const uint8_t led_pin = 4;
const uint8_t config_pin = 5;

const uint8_t ntpServerLen=128;
char ntpServerStr[ntpServerLen];
const uint8_t ntpIntervalLen = 8;
char ntpIntervalStr[ntpIntervalLen];
uint8_t ntpIntervalInt;

const uint8_t serialRateLen = 7;
char serialRateStr[serialRateLen];
uint32_t serialRateInt;

const uint8_t nmeaEnableLen = 2;
char zdaEnableStr[nmeaEnableLen];
char ggaEnableStr[nmeaEnableLen];
char gsaEnableStr[nmeaEnableLen];
char rmcEnableStr[nmeaEnableLen];

bool wifiIsConnected = false;

void wifiConnected();
void configSaved();
boolean formValidator();

DNSServer dnsServer;
WebServer server(80);

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, configVersion);
IotWebConfParameter ntpServerParam = IotWebConfParameter("NTP Server", "ntpServer", ntpServerStr, ntpServerLen);
IotWebConfParameter ntpInvervalParam = IotWebConfParameter("NTP Interval (hours)", "ntpInterval", ntpIntervalStr, ntpIntervalLen, "number", "1..168", "1", "min='1' max='168' step='1'");
IotWebConfParameter serialRateParam = IotWebConfParameter("Serial data rate", "serialRate", serialRateStr, serialRateLen, "number", "1200...115200", "9600", "min='1200' max='115200' step='1'");
IotWebConfParameter zdaEnableParam = IotWebConfParameter("Enable GPZDA", "zdaEnable", zdaEnableStr, nmeaEnableLen, "number", "0..1", "1", "min='0' max='1' step='1'");
IotWebConfParameter ggaEnableParam = IotWebConfParameter("Enable GPGGA", "ggaEnable", ggaEnableStr, nmeaEnableLen, "number", "0..1", "1", "min='0' max='1' step='1'");
IotWebConfParameter gsaEnableParam = IotWebConfParameter("Enable GPGSA", "gsaEnable", gsaEnableStr, nmeaEnableLen, "number", "0..1", "1", "min='0' max='1' step='1'");
IotWebConfParameter rmcEnableParam = IotWebConfParameter("Enable GPRMC", "rmcEnable", rmcEnableStr, nmeaEnableLen, "number", "0..1", "1", "min='0' max='1' step='1'");

void setup() {
  // put your setup code here, to run once:
  pinMode(led_pin, OUTPUT);
  pinMode(config_pin, INPUT);

  Serial.begin(9600);

  iotWebConf.setStatusPin(led_pin, HIGH);
  iotWebConf.setConfigPin(config_pin);
  iotWebConf.addParameter(&ntpServerParam);
  iotWebConf.addParameter(&ntpInvervalParam);
  iotWebConf.addParameter(&serialRateParam);
  iotWebConf.addParameter(&zdaEnableParam);
  iotWebConf.addParameter(&ggaEnableParam);
  iotWebConf.addParameter(&gsaEnableParam);
  iotWebConf.addParameter(&rmcEnableParam);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.init();

  configSaved();
  ntp_init();

  server.on("/", handle_root);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
}

void wifiConnected()
{
  iotWebConf.disableBlink();
  digitalWrite(led_pin, LOW);
  ArduinoOTA.begin();
  wifiIsConnected = true;
}

void configSaved()
{
  bool zda, gga, gsa, rmc;

  serialRateInt = atoi(serialRateStr);
  if (serialRateInt == 0) serialRateInt = 9600;
  Serial.end();
  Serial.begin(serialRateInt);
  ntpIntervalInt = atoi(ntpIntervalStr);
  ntp_config(ntpServerStr, ntpIntervalInt);

  zda = zdaEnableStr[0] == '1' ? true : false;
  gga = ggaEnableStr[0] == '1' ? true : false;
  gsa = gsaEnableStr[0] == '1' ? true : false;
  rmc = rmcEnableStr[0] == '1' ? true : false;
  nmea_init(zda, gga, gsa, rmc);
}

boolean formValidator()
{
  boolean valid = true;

  int r = server.arg(serialRateParam.getId()).toInt();
  switch (r)
  {
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
    {
      valid = true;
      break;
    }
    default:
    {
      serialRateParam.errorMessage = "Please use a valid serial data rate (1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)";
      valid = false;
      break;
    }
  }

  return valid;
}

void loop() {
  time_t timeUNIX;
  
  iotWebConf.doLoop();

  if ((WiFi.status() == WL_CONNECTED) && wifiIsConnected)
  {
    ArduinoOTA.handle();

    if (ntp_handle(&timeUNIX))
    {
      nmea_out(timeUNIX);
    }
  }
}

/**
 * Handle web requests to "/" path.
 */
void handle_root()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>ntpbee</title></head><body>Hello world!<br/>";
  s += "Go to <a href='config'>configure page</a> to change settings.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}
