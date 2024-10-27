#include <WiFi.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// WiFi and MQTT credentials
const char* ssid = "Magda en Karol";
const char* password = "klapeczki";
const char* mqttServer = "homeassistant.local";
const char* mqttUser = "Climate";
const char* mqttPassword = "klapeczki";
const char* mqttTopic = "home/bedroom/temperature";
const char* mqttClientId = "BedroomTemperature";  // each mqtt client needs a different id

// Define the pin for the OneWire bus (connected to the DS18B20 sensor)
#define ONE_WIRE_BUS 4

// Timing variables
unsigned long sensorStamp = 0;
unsigned long mqttStamp = 0;

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);

// OneWire and DallasTemperature objects
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  // Start the Serial Monitor
  Serial.begin(115200);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Set up MQTT server
  client.setServer(mqttServer, 1883);

  // Start the DS18B20 temperature sensor
  sensors.begin();
}

void reconnectWiFi() {
  // Attempt to reconnect to WiFi if the connection is lost
  Serial.println("WiFi connection lost. Reconnecting...");
  WiFi.reconnect();
  double timeout = millis() + 10000;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Reconnecting to WiFi...");
    if (millis() > timeout){
      Serial.println("Timeout connecting to wifi");
      return;
    }
  }
  Serial.println("Reconnected to WiFi");
}

void reconnectMQTT() {
  // Reconnect to the MQTT server
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqttClientId, mqttUser, mqttPassword)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(1000);  // Wait before retrying
    }
    
    // If WiFi is disconnected during MQTT reconnection, reconnect WiFi
    if (WiFi.status() != WL_CONNECTED) {
      break;
    }
  }
}

void loop() {
  // Check and reconnect WiFi if necessary
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi();
  }

  // Check and reconnect to the MQTT broker if necessary
  if (!client.connected()) {
    reconnectMQTT();
  }

  // Maintain MQTT connection
  client.loop();

  // Publish temperature to MQTT every 1 second
  if (millis() - mqttStamp > 1000) {
    mqttStamp = millis();
    sensors.requestTemperatures();  // Request temperature readings again
    float temp = sensors.getTempCByIndex(0);  // Get the temperature from the first sensor

    // Prepare temperature data as a string to publish
    char tempString[10];
    snprintf(tempString, sizeof(tempString), "%.2f", temp);

    if (temp > 0){
      // Publish the temperature data to the MQTT topic
      client.publish(mqttTopic, tempString);
    }

    Serial.print("Real Time Temp: ");
    Serial.println(tempString);
  }
}
