#include "FirebaseESP8266.h"
#include<ESP8266WiFi.h>
#include <time.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include "Adafruit_Si7021.h"
#include "sensitive.h"
#include <ArduinoOTA.h>

FirebaseData firebaseData;
FirebaseJson json;
ADC_MODE(ADC_VCC);

void printResult(FirebaseData &data);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

class SensorResult {
  public:
    SensorResult (float temp, float humid) {
      temperature = temp;
      humidity = humid;
    }
    float temperature;
    float humidity;
};

class Sensor {
  public:
    SensorResult ReadSensor() {
      Adafruit_Si7021 sensor = Adafruit_Si7021();
    	if (!sensor.begin()) {
        Serial.println("Did not find Si7021 sensor!");
        return SensorResult (0, 0);
  	  }

      auto humid = sensor.readHumidity();
      auto temp = sensor.readTemperature();
      return SensorResult (temp, humid);
    }
};


void setup()
{
  Serial.begin(115200);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println();
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  timeClient.begin();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);

  //Set the size of WiFi rx/tx buffers in the case where we want to work with large data.
  firebaseData.setBSSLBufferSize(1024, 1024);
  //Set the size of HTTP response buffers in the case where we want to work with large data.
  firebaseData.setResponseSize(1024);
  //tiny, small, medium, large and unlimited.
  //Size and its write timeout e.g. tiny (1s), small (10s), medium (30s) and large (60s).
  Firebase.setwriteSizeLimit(firebaseData, "tiny");

  ArduinoOTA.setHostname("ESP8266");
  ArduinoOTA.setPassword("esp8266");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA ready");
}

void updateTimeClient() {
  // Retry twice if the time client update wasn't successful
  if(!timeClient.update()) {
    delay(300);
    timeClient.update();
  }
}

void loop()
{
  
  ArduinoOTA.handle();

  Sensor s; 
  SensorResult data = s.ReadSensor();
  int voltage = ESP.getVcc();

  updateTimeClient();

  json.clear();

  json.add("epoch_time", String(timeClient.getEpochTime()));
  json.add("temperature", data.temperature);
  json.add("humidity", data.humidity);
  json.add("voltage", voltage);

  String jsonToStr;
  json.toString(jsonToStr, true);
  Serial.println(jsonToStr);

  if (Firebase.pushJSON(firebaseData, "/greenhouse/data_v2", json))
  {
    Serial.println("PASSED");
    Serial.println("PATH: " + firebaseData.dataPath());
    Serial.print("PUSH NAME: ");
    Serial.println(firebaseData.pushName());
    Serial.println("ETag: " + firebaseData.ETag());
    Serial.println();
  }
  else
  {
    Serial.println("FAILED");
    Serial.println("REASON: " + firebaseData.errorReason());
    Serial.println("------------------------------------");
    Serial.println();
  }

  Serial.println("Going to sleep...");
  int64_t microseconds = 3.6e9;
  ESP.deepSleep(microseconds);
  delay(1000);
}