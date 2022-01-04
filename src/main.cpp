#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "DateTimeMS.h"
#include "EspMQTTClient.h"
#include <ArduinoJson.h>

#include "CredentialSetting.h"
#include "CredentialSettingsDefault.h"

#include "Version.h"

#define debug

#define SENSORINPUT 0

#define SWVersion "V" VERSION
#define DEVICENAME "Wasserwächter"
String deviceName = DEVICENAME;

const char *ssid = STASSID;
const char *password = STAPSK;

EspMQTTClient MQTTClient(
    ssid,
    password,
    MQTTHostname, // MQTT Broker server ip
    MQTTUser,
    MQTTPassword,
    deviceName.c_str(), // Client name that uniquely identify your device
    MQTTPort            // The MQTT port, default to 1883
);

unsigned long UpdateIntervall = 5000; // 5s update intervall
unsigned long nextUpdateTime = 0;     // calculated time in millis for next update

unsigned long waterCounter = 0;     // number of rising edges of the external sensor
unsigned long lastDuration = 0;     // duration of the last pulse measured
unsigned long lastChangeTime = 0;   // timestamp of the last rising sensor
unsigned int impulsesPerLiter = 16; // number of impulses per liter
float flowRate = 0.0;               // flow rate calculated from duration per puls in l/s

/**
* @brief Interrupt routine for externel Sensor input
* 
* measure duration of last puls
* measure timestamp of rising ping
* count rising pin
*/
void IRAM_ATTR measureSensor()
{
  unsigned long actTime = millis();

  lastChangeTime = actTime;
  lastDuration = actTime - lastChangeTime;
  flowRate = impulsesPerLiter / lastDuration * 1000.0;

  waterCounter++;
}

/**
 * @brief Arduino setup function
 * initialise the device at the beginning
 */
void setup()
{
  //init absolut time variables
  unsigned long actTime = millis();
  nextUpdateTime = actTime + UpdateIntervall;
  lastChangeTime = actTime;

  //init serial port for debug
  Serial.begin(115200);
  Serial.println("Booting");

  // init digital IOs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSORINPUT, INPUT);
  attachInterrupt(digitalPinToInterrupt(SENSORINPUT), measureSensor, RISING);

  // init MQTT client
#ifdef debug
  MQTTClient.enableDebuggingMessages(); // Enable debugging messages sent to serial output
#endif
  MQTTClient.enableLastWillMessage("TestClient/lastwill", "I am going offline"); // You can activate the retain flag by setting the third parameter to true
}

/**
 * @brief resume setup after WIFI connection
 * This function will setup all services and is called once everything is connected (Wifi and MQTT)
 */
void onConnectionEstablished()
{
  Serial.println("onConn");
  delay(1000);

  // announce that the device is online again in the cloud
  MQTTClient.publish("device/online", deviceName.c_str());

  // subscribe to device scan channel
  MQTTClient.subscribe("device", [](const String &payload)
                       {
                         if (payload == "scan")
                         {
                           MQTTClient.publish("device/scan", deviceName.c_str());
                         }
                       });
}

/**
 * @brief send Data to Cloud (MQTT)
 * 
 * @return true for successful publishing
 */
bool sendNewData()
{
  String message; // will contain the http message to send into cloud

  // Publish a message to "mytopic/test"
  message = "{\"name\":\"" DEVICENAME "\",\"field\":\"Water\",\"ChangeCounter\":";
  message += waterCounter;
  message += ",\"timeSinceChange\":";
  message += (millis() - lastChangeTime);
  message += ",\"lastDuration\":";
  message += lastDuration;
  message += ",\"time\":";
  message += DateTimeMS.osTimeMS();
  message += "}";
  return MQTTClient.publish("sensors", message);
}

/**
 * @brief Arduino loop
 * 
 */
void loop()
{
  MQTTClient.loop(); // Handle MQTT

  if (MQTTClient.isConnected())
  {

    // test if time is still valid
    if (!DateTime.isTimeValid())
    {
      DateTime.begin(/* timeout param */);
    }
    else if (millis() > nextUpdateTime)
    {
      nextUpdateTime += UpdateIntervall;
      sendNewData();
    }
  }
}
