#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "DateTimeMS.h"
#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "CredentialSetting.h"

#include "Version.h"

#define debug

#define DEVICENAME "TestOldData"
#define SWVersion "V" VERSION
String deviceName = DEVICENAME;


#ifndef STASSID 
#define STASSID "..."
#endif
#ifndef STAPSK
#define STAPSK "..."
#endif
const char* ssid     = STASSID;
const char* password = STAPSK;

#ifndef MQTTHostname
#define MQTTHostname "mqtt.host.com"
#endif
#ifndef MQTTUser
#define MQTTUser "username"
#endif
#ifndef MQTTPassword
#define MQTTPassword "password"
#endif
#ifndef MQTTPort
#define MQTTPort 1883
#endif
EspMQTTClient MQTTClient(
  ssid,
  password,
  MQTTHostname,       // MQTT Broker server ip
  MQTTUser,
  MQTTPassword,
  deviceName.c_str(),  // Client name that uniquely identify your device
  MQTTPort           // The MQTT port, default to 1883
);

unsigned long UpdateIntervall = 5000;    // 1 minute Update
unsigned long nextUpdateTime = 0;
unsigned long getNextUpdateTime() { return millis() + UpdateIntervall; };


// -----------------------------------------------------------------
// initialise the device at the beginning
// -----------------------------------------------------------------
void setup() {
  nextUpdateTime = getNextUpdateTime();
  Serial.begin(115200);
  Serial.println("Booting");

#ifdef debug
  MQTTClient.enableDebuggingMessages(); // Enable debugging messages sent to serial output
#endif
  MQTTClient.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true
}

// -----------------------------------------------------------------
// This function will setup all services and is called once everything is connected (Wifi and MQTT)
// -----------------------------------------------------------------
void onConnectionEstablished()
{
  Serial.println("onConn");
  delay(1000);

  // announce that the device is online again in the cloud
  MQTTClient.publish("device/online", deviceName.c_str());

  // subscribe to device scan channel
  MQTTClient.subscribe("device", [](const String & payload) {
    if (payload == "scan") {
      MQTTClient.publish("device/scan", deviceName.c_str());
    }
  });
}

// -----------------------------------------------------------------
// get Time String with fake ms (000)
// -----------------------------------------------------------------
String getStringTimeWithMS() {
  String strTime = "";
  strTime += DateTimeMS.osTimeMS();
  return strTime;
}

// -----------------------------------------------------------------
// send Data to Cloud (ThingSpeak and MQTT)
// -----------------------------------------------------------------
void sendNewData() {
  String message;                           // will contain the http message to send into cloud

  // Publish a message to "mytopic/test"
  message = "{\"name\":\"" DEVICENAME "\",\"field\":\"Test\",\"Value\":";
  message += 1.0;
  message += ",\"time\":";
  message += getStringTimeWithMS();
  message += "}";
  MQTTClient.publish("sensors", message); // You can activate the retain flag by setting the third parameter to true
}


// -----------------------------------------------------------------
// Loop
// -----------------------------------------------------------------
void loop() {
  MQTTClient.loop();          // Handle MQTT

  if (MQTTClient.isConnected()) {

    // test if time is still valid
    if (!DateTime.isTimeValid()) {
      DateTime.begin(/* timeout param */);
    } else if (millis() > nextUpdateTime) {
      sendNewData();      
      nextUpdateTime = getNextUpdateTime();
    }
  }
}

