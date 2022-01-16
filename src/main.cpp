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

#define DEVICENAME "Wasserwächter" 
String deviceName = DEVICENAME;

const char *ssid = STASSID;
const char *password = STAPSK;

#define SENSORINPUT 0             ///< pin number for digital sensor input
#define SENSORANALOG A0           ///< pin number for analog sensor input
#define MAXSAMPLES 32             ///< array size for analog samples
int sensorValues[MAXSAMPLES];     ///< Array to store samples from analog input pin
int sensorValueIndex = 0;         ///< array index pointing to the next free element
int postTrigger = MAXSAMPLES / 2; ///< number of post trigger samples

bool sampleStarted = false;       ///< signal to start sampling analog signal, set after sending data
#define TIMER_INTERVAL_MS 10      ///< sampling timer interval
ESP8266Timer ITimer;              ///< HW timer from ESP8266

/**
 * @brief MQTT client managing all network functions incl. WIFI connection
 * 
 * @param wifiSsID WIFI SSID to connect with
 * @param wifiPassword WIFI password to connect with
 * @param mqttServerIp MQTT Broker server IP
 * @param mqttUsername MQTT username
 * @param mqttPassword  MQTT password
 * @param mqttClientName Client name that uniquely identify your device
 * @param mqttServerPort MQTT port
 */
EspMQTTClient MQTTClient(
    ssid,      
    password,           
    MQTTHostname,
    MQTTUser,
    MQTTPassword,       
    deviceName.c_str(), 
    MQTTPort            
);

unsigned long UpdateIntervall = 5000; ///< update interval, measurement will be sent after each interval
unsigned long nextUpdateTime = 0;     ///< calculated absolute time in milliseconds for next update

unsigned long waterCounter = 0;     ///< number of rising edges of the external sensor
unsigned long lastDuration = 0;     ///< duration of the last measured pulse
unsigned long actDuration = 0;      ///< dynamically calculated duration 
                                    /**< if the actual puls is longer than the last duration \n 
                                     * this variable dynamically increases its value even there was no complete pulse
                                    */

unsigned long lastChangeTime = 0;   ///< absolute timestamp of the last rising edge of the sensor pulse
unsigned int impulsesPerLiter = 16; ///< number of impulses per Liter
float flowRate = 0.0;               ///< flow rate calculated from \p impulsesPerLiter and \p lastDuration in l/s
float flowRateFiltered = 0.0;       ///< filter to show decay even without input pulse
float filter = 0.9;                 ///< filter value (always < 1)

/**
* @brief Interrupt routine for external Sensor input
* 
* measure duration of last pulse \n 
* measure timestamp of rising ping \n 
* count rising pin
*/
void IRAM_ATTR measureSensor()
{

  sampleStarted = false;
  auto actTime = millis();
  lastDuration = actTime - lastChangeTime;
  lastChangeTime = actTime;
  flowRate = (float)impulsesPerLiter / (float)lastDuration * 1000.0;
  waterCounter++;
#ifdef debug
//  sensorValues[sensorValueIndex-1] = -1;
#endif
}

/**
 * @brief high speed sampling of analog signal \n 
 * timer interrupt based sampling of the analog input value
 * stored in an sampling array with pre and post trigger. \n 
 * pre trigger is filled when sampleStarted == true \n 
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
