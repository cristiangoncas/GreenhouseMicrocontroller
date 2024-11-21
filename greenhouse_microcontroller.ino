#include <dht11.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ctime>
#include <ArduinoJson.h>
#include "secrets.h"

#define DHT_PIN 2
const int heaterPin = 13;

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *serverName = SERVER_IP;
const long interval = 60000;

unsigned int defaultMinTemp = 13;
unsigned int defaultMaxTemp = 18;
unsigned int defaultMorningTime = 9;
unsigned int defaultNightTime = 19;
unsigned int defaultNightTempDifference = 5;
unsigned int nightTempDifference = defaultNightTempDifference;
unsigned int minTemp = defaultMinTemp;
unsigned int maxTemp = defaultMaxTemp;
unsigned int dayMinTemp = minTemp;
unsigned int dayMaxTemp = maxTemp;
unsigned int nightMinTemp = minTemp - nightTempDifference;
unsigned int nightMaxTemp = maxTemp - nightTempDifference;
unsigned int morningTime = defaultMorningTime;
unsigned int nightTime = defaultNightTime;
unsigned int heartbeatPeriod = 5;
unsigned long pM = 0;
unsigned long iC = 0;
bool isDebug = false;
unsigned long timeoutMillis = 60000;

dht11 DHT11;
struct tm timeinfo;

// TODO: Extract logic into separate files, network, sensors, api, etc.
// TODO: Improve error handling. The greenhouse should never fall under the defined parameters to avoid killing the plants.

void setup() {
  Serial.begin(9600);
  pinMode(heaterPin, OUTPUT);

  setupWifi();
  getLocalTime(&timeinfo);
  setupTime();
}

void loop() {
  // Loops every minute
  unsigned long currentMillis = millis();

  if (currentMillis - pM >= interval) {
    // Every minute
    pM = currentMillis;

    // Check heartbeat
    if (iC % heartbeatPeriod == 0) {
      Serial.print("HeartBeat!");
      heartBeat();
    }
  
    // Check if it's night every hour
    if (iC % 60 == 0 || iC == 0) {
      if (isNight()) {
        minTemp = nightMinTemp;
        maxTemp = nightMaxTemp;
      } else {
        minTemp = dayMinTemp;
        maxTemp = dayMaxTemp;
      }
    }

    // Check temperature every 1 min
    if (iC % 1 == 0) {
      float temperature = readTemperature();
      float humidity = readHumidity();
      handleTemperature(temperature);
      // handleHumidity(humidity);

      // Send log every 10 min
      if (iC % 10 == 0 || iC == 0) {
        String logs[][2] = {
          {"tempRead", String(temperature)},
          {"humidRead", String(humidity)}
        };
        int numLogs = sizeof(logs) / sizeof(logs[0]);
        sendLog(logs, numLogs);
      }
    }

    if (iC == 60) {
      iC = 1;
    } else {
      iC++;
    }
  }
}

void setupWifi() {
  int counter = 0;
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED && counter < 20)
  {
    unsigned long currentMillis = millis();
    if (currentMillis - pM >= 500) {
      Serial.print(".");
      counter++;
    }
  }
  Serial.println(" Connected");
}

void getLocalTime() {
  if (!getLocalTime(&timeinfo)) {
    ESP.restart();
    return;
  }
}

void setupTime() {
  configTime(-5 * 3600, 3600, "pool.ntp.org", "time.nist.gov");

  getLocalTime();
  char timeStr[64];
  strftime(timeStr, sizeof(timeStr), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  String logs[][2] = {
    {"setTime", String(timeStr)}
  };
  int numLogs = sizeof(logs) / sizeof(logs[0]);
  sendLog(logs, numLogs);
}

float readTemperature() {
  // Read and return temperature
  int chk = DHT11.read(DHT_PIN);
  int temp = DHT11.temperature;
  if (isDebug) {
    return 16;
  } else {
    return temp;
  }
}

float readHumidity() {
  // Read and return humidity
  int chk = DHT11.read(DHT_PIN);
  int h = DHT11.humidity;
  if (isDebug) {
    return 70;
  } else {
    return h;
  }
}

void handleTemperature(int temperature) {
  // Executes an action based on the temperature
  int heaterStatus = digitalRead(heaterPin);
  if (temperature >= maxTemp && heaterStatus == HIGH) {
    String logs[][2] = {
      {"heater", "Off"}
    };
    int numLogs = sizeof(logs) / sizeof(logs[0]);
    sendLog(logs, numLogs);
    digitalWrite(heaterPin, LOW);
    Serial.println("Turn off heater");
  } else if (temperature <= minTemp && heaterStatus == LOW) {
    String logs[][2] = {
      {"heater", "On"}
    };
    int numLogs = sizeof(logs) / sizeof(logs[0]);
    sendLog(logs, numLogs);
    digitalWrite(heaterPin, HIGH);
    Serial.println("Turn on heater");
  } else if (temperature >= minTemp+1 && heaterStatus == HIGH) {
    String logs[][2] = {
      {"heater", "Off"}
    };
    int numLogs = sizeof(logs) / sizeof(logs[0]);
    sendLog(logs, numLogs);
    digitalWrite(heaterPin, LOW);
    Serial.println("Turn off heater");
  }    
}

bool isNight() {
  getLocalTime();
  int hour = timeinfo.tm_hour;
  int min = timeinfo.tm_min;
  int sec = timeinfo.tm_sec;
  if (hour >= nightTime || hour < morningTime) {
    String logs[][2] = {
      {"isNight", String(1)}
    };
    int numLogs = sizeof(logs) / sizeof(logs[0]);
    sendLog(logs, numLogs);
    return true;
  } else {
    String logs[][2] = {
      {"isNight", String(0)}
    };
    int numLogs = sizeof(logs) / sizeof(logs[0]);
    sendLog(logs, numLogs);
    return false;
  }
}

void sendLog(String logs[][2], int numLogs) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(String(serverName) + "/registerLog");
    http.addHeader("Content-Type", "application/json");
    
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.createNestedArray("logs");
    
    for(int i = 0; i < numLogs; i++) {
      JsonObject obj = array.createNestedObject();
      obj["event"] = logs[i][0];
      obj["data"] = logs[i][1];
    }
    
    String requestBody;
    serializeJson(doc, requestBody);
    
    int httpResponseCode = http.POST(requestBody);
    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println("HTTP Response code: " + httpResponseCode);
      Serial.println("Response: " + response);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
  }
}

void heartBeat() {
  if (WiFi.status() == WL_CONNECTED) {  //Check WiFi connection status
    HTTPClient http;    //Declare an object of class HTTPClient

    http.begin(String(serverName) + "/heartBeat");  // Specify request destination
    int httpCode = http.GET();  //Send the request

    if (httpCode > 0) { //Check the returning code

      String payload = http.getString();  //Get the request response payload
      // Parse JSON (assuming the JSON structure that you know, e.g., {"maxTemp":24,"minTemp":18})
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, payload);
      if (doc.size() >= 1) {
        // Check maxTemp
        if (doc.containsKey("maxTemp")) {
          maxTemp = doc["maxTemp"].as<int>();
          dayMaxTemp = maxTemp;
          nightMaxTemp = maxTemp - nightTempDifference;
        }
        // Check minTemp
        if (doc.containsKey("minTemp")) {
          minTemp = doc["minTemp"].as<int>();
          dayMinTemp = minTemp;
          nightMinTemp = minTemp - nightTempDifference;
        }
        // Check morningTime
        if (doc.containsKey("morningTime")) {
          morningTime = doc["morningTime"].as<int>();
        }
        // Check nightTime
        if (doc.containsKey("nightTime")) {
          nightTime = doc["nightTime"].as<int>();
        }
        // Check nightTempDifference
        if (doc.containsKey("nightTempDifference")) {
          nightTempDifference = doc["nightTempDifference"].as<int>();
          nightMaxTemp = maxTemp - nightTempDifference;
          nightMinTemp = minTemp - nightTempDifference;
        }
        // Check heartbeatPeriod
        if (doc.containsKey("heartbeatPeriod")) {
          heartbeatPeriod = doc["heartbeatPeriod"].as<int>();
        }
        // Check resetDefaults
        if (doc.containsKey("resetDefaults")) {
          maxTemp = defaultMaxTemp;
          minTemp = defaultMinTemp;
          morningTime = defaultMorningTime;
          morningTime = defaultNightTime;
          nightTempDifference = defaultNightTempDifference;
          dayMaxTemp = maxTemp;
          dayMinTemp = minTemp;
          nightMaxTemp = maxTemp - nightTempDifference;
          nightMinTemp = minTemp - nightTempDifference;
        }
        // Check healthCheck
        if (doc.containsKey("healthCheck")) {
          String logs[][2] = {
            {"maxTemp", String(maxTemp)},
            {"minTemp", String(minTemp)},
            {"morningTime", String(morningTime)},
            {"nightTime", String(nightTime)},
            {"nightTempDifference", String(nightTempDifference)}
          };
          int numLogs = sizeof(logs) / sizeof(logs[0]);
          sendLog(logs, numLogs);
        }

        String logs[][2] = {
          {"heartBeat", payload}
        };
        int numLogs = sizeof(logs) / sizeof(logs[0]);
        sendLog(logs, numLogs);
      }
    } else {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpCode);
    }

    http.end(); //Free the resources
  }
}
