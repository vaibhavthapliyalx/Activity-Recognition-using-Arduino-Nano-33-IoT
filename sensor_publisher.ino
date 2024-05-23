#include <SPI.h>
#include <WiFiNINA.h>
#include <ArduinoMqttClient.h>
#include <Arduino_LSM6DS3.h>
#include <ArduinoJson.h>

// WiFi credentials (replace with your credentials)
const char* uni_ssid = "Vaibhav's iPhone";
const char* uni_pass = "12345678";

const char* home_ssid = "Home-WiFi";
const char* home_pass = "forSpice123";

char payload[1024];
DynamicJsonDocument doc(1024);

const char* activities[] = {"Walking", "Clapping", "Drinking", "Eating", "Pouring", "Sitting"}; // Activity labels
int currentActivity = 0;

// MQTT Broker settings
const char broker[] = "192.168.1.54"; // IP address of my MQTT broker (My Macbook)
int port = 1883;
const char topic[]  = "com669/vaibhav";
 
WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);

void setup() {
  Serial.begin(9600);
  // Initialize IMU
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU!");
    while (1);
  }

  // Connect to WiFi
  connectToWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Wifi Connected Successfully");
  }

  // Connect to MQTT Broker (optional for this stage)
  connectToMqtt();
}

void connectToMqtt() {
  Serial.print("Connecting to MQTT...");
  Serial.println("Broker and port below");
  Serial.println(broker);
  Serial.println(port);
  Serial.println(mqttClient.connect(broker, port));
  while (!mqttClient.connect(broker, port)) {
    Serial.println("Retrying.....MQTT");
    Serial.print(".");
    delay(1000);
  }
  mqttClient.beginMessage(topic);
  mqttClient.print("Connected to MQTT Broker successfully!");
  mqttClient.print("Sensor data collection started...");
  mqttClient.endMessage();
}

void connectToWiFi() {
  // Connect to WiFi
  WiFi.begin(uni_ssid, uni_pass);

  bool connected = false;

  while (!connected) {
    int numNetworks = WiFi.scanNetworks();
    if (numNetworks == -1) {
      Serial.println("Failed to scan networks. Retrying...");
      delay(1000);
      continue;
    }

    for (int i = 0; i < numNetworks; i++) {
      String networkName = WiFi.SSID(i);
      if (networkName.equals(uni_ssid)) {
        Serial.println("Connecting to University WiFi...");
        WiFi.begin(uni_ssid, uni_pass);
        connected = true;
        break;
      } else if (networkName.equals(home_ssid)) {
        Serial.println("Connecting to Home WiFi...");
        WiFi.begin(home_ssid, home_pass);
        connected = true;
        break;
      }
    }

    if (!connected) {
      Serial.println("Desired networks not found. Retrying...");
      delay(1000);
    } else {
      Serial.println("Connecting to WiFi...");
    }

    delay(5000); // Wait for WiFi connection
  }

  Serial.println("WiFi Connected!");
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMqtt();
  }
  mqttClient.poll();

  // Announce the start of the new activity
  char activityMessage[50];
  sprintf(activityMessage, "Recording Activity %d - %s. Please begin %s in 30 seconds...", currentActivity + 1, activities[currentActivity], activities[currentActivity]);
  mqttClient.beginMessage(topic);
  mqttClient.print(activityMessage);
  mqttClient.endMessage();

  // 15 seconds countdown before the activity starts
  for (int i = 15; i > 0; i--) {
    char countdownMessage[50];
    sprintf(countdownMessage, "Activity starts in: %d seconds", i);
    mqttClient.beginMessage(topic);
    mqttClient.print(countdownMessage);
    mqttClient.endMessage();

    delay(1000);
  }

  // Collect data for 5 bursts of 30 seconds each
  for (int burst = 0; burst < 5; burst++) {
    char burstMessage[50];
    sprintf(burstMessage, "Burst: %d (30 seconds)", burst + 1);
    mqttClient.beginMessage(topic);
    mqttClient.print(burstMessage);
    mqttClient.endMessage();

    // Reset timer for each burst
    unsigned long startTime = millis();

    while (millis() - startTime < 30000) { // Loop for 30 seconds
      float ax, ay, az, gx, gy, gz;

      if (IMU.accelerationAvailable() && IMU.gyroscopeAvailable()) {
        IMU.readAcceleration(ax, ay, az);
        IMU.readGyroscope(gx, gy, gz);

        // Convert data to JSON
        doc["activity"] = activities[currentActivity]; // Activity label
        doc["timestamp"] = millis(); // Current timestamp
        doc["ax"] = ax;
        doc["ay"] = ay;
        doc["az"] = az;
        doc["gx"] = gx;
        doc["gy"] = gy;
        doc["gz"] = gz;
        doc["burst_count"] = burst;

        // Serialize JSON
        size_t written = serializeJson(doc, payload);
        if (written == 0) {
          char errorMessage[50];
          sprintf(errorMessage, "Serialization failed. Skipping data point...");
          mqttClient.beginMessage(topic);
          mqttClient.print(errorMessage);
          mqttClient.endMessage();
          continue;
        }

        // Publish data
        if (!mqttClient.connected()) {
          connectToMqtt();
        }
        mqttClient.beginMessage(topic);
        mqttClient.print(payload);
        if(!mqttClient.endMessage()) {
          Serial.println("Failed to send MQTT message!");
        }
      }
      // 50 Hz data collection
      delay(20);
    }

    // 10 seconds delay between bursts with countdown
    for (int i = 5; i > 0; i--) {
      char countdownMessage[50];
      sprintf(countdownMessage, "Next burst starts in: %d seconds", i);
      mqttClient.beginMessage(topic);
      mqttClient.print(countdownMessage);
      mqttClient.endMessage();

      delay(1000);
    }
  }
  mqttClient.beginMessage(topic);
  mqttClient.print("Data collection complete!");
  mqttClient.endMessage();

  // Stop collecting data after 6 activities
  if (currentActivity == 5) {
    mqttClient.beginMessage(topic);
    mqttClient.print("Data collection complete for all activities!");
    mqttClient.endMessage();
    while (1);
  }
  // Switch to next activity
  currentActivity = (currentActivity + 1) % 6;
}