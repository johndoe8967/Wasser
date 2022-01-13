#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "DateTimeMS.h"
#include "EspMQTTClient.h"
#include <ArduinoJson.h>
#include "ESP8266TimerInterrupt.h"

#include "CredentialSetting.h"
#include "CredentialSettingsDefault.h"

#include "Version.h"

#define debug

#define SENSORINPUT 0
#define SENSORANALOG A0
#define MAXSAMPLES 32
int sensorValues[MAXSAMPLES];    // Array to store samples from analog input pin
int sensorValueIndex = 0;
int postTrigger = MAXSAMPLES / 2;

bool sampleStarted = false;
#define TIMER_INTERVAL_MS        10
ESP8266Timer ITimer;

#define SWVersion "V" VERSION
#define DEVICENAME "Wasserwächter"
String deviceName = DEVICENAME;

const char *ssid = STASSID;
const char *password = STAPSK;

EspMQTTClient MQTTClient(
    ssid,
    password,
    MQTTHostname, // MQTT Broker server IP
    MQTTUser,
    MQTTPassword,
    deviceName.c_str(), // Client name that uniquely identify your device
    MQTTPort            // The MQTT port, default to 1883
);

unsigned long UpdateIntervall = 5000; // 5s update interval
unsigned long nextUpdateTime = 0;     // calculated time in milliseconds for next update

unsigned long waterCounter = 0;     // number of rising edges of the external sensor
unsigned long actTime = 10; 
unsigned long lastDuration = 0;     // duration of the last pulse measured
unsigned long actDuration = 0;
unsigned long lastChangeTime = 0;   // timestamp of the last rising sensor
unsigned int impulsesPerLiter = 16; // number of impulses per Liter
float flowRate = 0.0;               // flow rate calculated from duration per pulse in l/s
float flowRateFiltered = 0.0;
float filter = 0.9;

/**
* @brief Interrupt routine for external Sensor input
* 
* measure duration of last pulse
* measure timestamp of rising ping
* count rising pin
*/
void IRAM_ATTR measureSensor()
{
  sampleStarted = false;
  actTime = millis();
  lastDuration = actTime - lastChangeTime;
  lastChangeTime = actTime;
  flowRate = (float)impulsesPerLiter / (float)lastDuration * 1000.0;
  waterCounter++;
#ifdef debug
//  sensorValues[sensorValueIndex-1] = -1;
#endif
}

/**
 * @brief high speed sampling of analog signal 
 * timer interrupt based sampling of the analog input
 * stored in an sampling array with pre and post trigger. 
 * pre trigger is filled when sampleStarted == true
 * post trigger is filled after sampleStarted == false
 */
void IRAM_ATTR sampleAnalogSignal()
{
  if (sampleStarted || postTrigger!=0) {
#ifdef debug
    sensorValues[sensorValueIndex++] = sensorValueIndex;
#else
    sensorValues[sensorValueIndex++] = analogRead(SENSORANALOG);
#endif
    sensorValueIndex &= (MAXSAMPLES - 1);
  } else {
    if (postTrigger != 0) postTrigger--;
  }
}



/**
 * @brief Arduino setup function
 * initialize the device at the beginning
 */
void setup()
{
  //initialize absolute time variables
  unsigned long actTime = millis();
  nextUpdateTime = actTime + UpdateIntervall;
  lastChangeTime = actTime;
  sensorValueIndex = 0;

  //initialize serial port for debug
  Serial.begin(115200);
  Serial.println("Booting");

  // init digital IOs
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(SENSORINPUT, INPUT);
  attachInterrupt(digitalPinToInterrupt(SENSORINPUT), measureSensor, RISING);

  // attach timer interrupt for high speed analog signal sampling
  if (ITimer.attachInterruptInterval(TIMER_INTERVAL_MS * 1000, sampleAnalogSignal))
  {
    Serial.println(F("Starting  ITimer OK")); 
  }
  else
    Serial.println(F("Can't set ITimer correctly. Select another freq. or interval"));


  // init MQTT client
#ifdef debug
  MQTTClient.enableDebuggingMessages(); // Enable debugging messages sent to serial output
#endif
  MQTTClient.enableLastWillMessage("TestClient/lastwill", "I am going offline"); // You can activate the retain flag by setting the third parameter to true
  MQTTClient.setMaxPacketSize(2500);
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
bool sendNewData(uint64_t osTimeMS)
{
  bool sendOK = true;
  String message; // will contain the http message to send into cloud

  // Publish a message to "mytopic/test"
  message = "{\"name\":\"" DEVICENAME "\",\"field\":\"Water\",\"ChangeCounter\":";
  message += waterCounter;
  message += ",\"timeSinceChange\":";
  message += actDuration;
  message += ",\"lastDuration\":";
  message += lastDuration;
  message += ",\"flowRate\":";
  message += flowRateFiltered;
  message += ",\"time\":";
  message += osTimeMS;
  message += "}";
  if (!MQTTClient.publish("sensors", message)) sendOK = false;

  auto actTime = osTimeMS - (millis() - lastChangeTime);

  if (sampleStarted == false) {
    message = "[";
    for (int i=0;i<MAXSAMPLES;i++) {
      message += "{\"name\":\"" DEVICENAME "\",\"field\":\"Analog\"";
      message += ",\"Value\":";
      message += sensorValues[(sensorValueIndex + i) & (MAXSAMPLES-1)];
      message += ",\"time\":";
      message += actTime - (MAXSAMPLES/2 - i) * 10;
      message += "}";
      if (i< MAXSAMPLES-1){
        message += ",";
      }
    }
    message += "]";
#ifdef debug
    Serial.print("Msg len:");
    Serial.println(message.length());
#endif
    if (!MQTTClient.publish("sensors", message)) sendOK = false;
    sampleStarted = true;
    postTrigger = MAXSAMPLES / 2;
  }

  return sendOK;
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
      actDuration = (millis() - lastChangeTime);
      if (actDuration > lastDuration) {
        flowRate = (float)impulsesPerLiter / (float)actDuration * 1000.0;
      } 
      flowRateFiltered = flowRateFiltered * filter + flowRate * (1-filter);
      nextUpdateTime += UpdateIntervall;
      sendNewData(DateTimeMS.osTimeMS());
    }
  }
}
