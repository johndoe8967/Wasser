#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <DateTime.h>
#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "CredentialSetting.h"

#define DEVICENAME "TestOldData"
#define SWVersion "V1.0"
String deviceName = DEVICENAME;


const char* ssid     = STASSID;
const char* password = STAPSK;

#define MQTT
#ifdef MQTT
EspMQTTClient MQTTClient(
  ssid,
  password,
  MQTTHostname,       // MQTT Broker server ip
  MQTTUser,
  MQTTPassword,
  deviceName.c_str(),  // Client name that uniquely identify your device
  MQTTPort           // The MQTT port, default to 1883
);
#endif

unsigned long UpdateIntervall = 5000;    // 1 minute Update
unsigned long myTime = 0;
bool connected = false;                   // connection established and all services initialiced?
bool sendValid = false;                   // new data available to send?
String message;                           // will contain the http message to send into cloud



// -----------------------------------------------------------------
// initialise the device at the beginning
// -----------------------------------------------------------------
void setup() {
  int cnt = 0;
  myTime = millis();

  Serial.begin(9600);
  Serial.println("Booting");

  //  MQTTClient.enableDebuggingMessages(); // Enable debugging messages sent to serial output
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

  connected = true;
}

// -----------------------------------------------------------------
// get Time String with fake ms (000)
// -----------------------------------------------------------------
String getStringTimeWithMS() {
  String strTime = "";
  strTime += DateTime.now();
  strTime += "000";
  return strTime;
}

// -----------------------------------------------------------------
// send Data to Cloud (ThingSpeak and MQTT)
// -----------------------------------------------------------------
void sendNewData() {
  String requestUrl;

  // Publish a message to "mytopic/test"
  message = "{\"name\":\"" DEVICENAME "\",\"field\":\"Test\",\"Value\":";
  message += 1.0;
  message += ",\"time\":";
  message += getStringTimeWithMS();
  message += "}";
  MQTTClient.publish("sensors", message); // You can activate the retain flag by setting the third parameter to true
  Serial.println("...Sent");
}

// -----------------------------------------------------------------
// Loop
// -----------------------------------------------------------------
String inputString = "";         // a String to hold incoming data
bool stringComplete = false;  // whether the string is complete
void loop() {
  MQTTClient.loop();          // Handle MQTT

  if (connected) {

    // test if time is still valid
    // set time zone to UTC

    if (!DateTime.isTimeValid()) {
      DateTime.begin(/* timeout param */);
    }

    if (millis() > myTime)
    {
      Serial.print("Send");
      sendNewData();
      myTime = millis() + UpdateIntervall;
    }
  }
}

