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

bool wifiIsConnected = false;

void wifiConnected();
void configSaved();

DNSServer dnsServer;
WebServer server(80);

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, configVersion);
IotWebConfParameter ntpServerParam = IotWebConfParameter("NTP Server", "ntpServer", ntpServerStr, ntpServerLen);
IotWebConfParameter ntpInvervalParam = IotWebConfParameter("NTP Interval (hours)", "ntpInterval", ntpIntervalStr, ntpIntervalLen, "number", "1..168", NULL, "min='1' max='168' step='1'");

void setup() {
  // put your setup code here, to run once:
  pinMode(led_pin, OUTPUT);
  pinMode(config_pin, INPUT);

  Serial.begin(9600); // FIXME make this configurable in web config

  iotWebConf.setStatusPin(led_pin, HIGH);
  iotWebConf.setConfigPin(config_pin);
  iotWebConf.addParameter(&ntpServerParam);
  iotWebConf.addParameter(&ntpInvervalParam);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.init();

  ntpIntervalInt = atoi(ntpIntervalStr);
  
  ntp_init();
  ntp_config(ntpServerStr, ntpIntervalInt);

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
  ntpIntervalInt = atoi(ntpIntervalStr);
  ntp_config(ntpServerStr, ntpIntervalInt);
}

void loop() {
  time_t timeUNIX;
  
  iotWebConf.doLoop();

  if ((WiFi.status() == WL_CONNECTED) && wifiIsConnected)
  {
    ArduinoOTA.handle();

    if (ntp_handle(&timeUNIX))
    {
      gen_nmea(timeUNIX);
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
  s += "<title>IotWebConf 02 Status and Reset</title></head><body>Hello world!<br/>";
  s += "Go to <a href='config'>configure page</a> to change settings.";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}
