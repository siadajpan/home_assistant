#include <WiFi.h>
#include <PubSubClient.h>

#define SSID ""Magda en Karol""
#define WIFI_PASSWORD "klapeczki"

#define MQTT_SERVER "homeassistant.local"
#define USER "Climate"
#define USER_PASSWORD "klapeczki"

#define MQTT_TOPIC_KITCHEN_PUMP_STATE "home/kitchen_pump/state"
#define MQTT_TOPIC_KITCHEN_PUMP_CONTROL "home/kitchen_pump/control"

#define MQTT_TOPIC_BEDROOM_PUMP_STATE "home/bedroom_pump/state"
#define MQTT_TOPIC_BEDROOM_PUMP_CONTROL "home/bedroom_pump/control"

#define PUMP_KITCHEN 18
#define PUMP_BEDROOM 16

WiFiClient espClient;
PubSubClient client(espClient);

bool pumpState = false; // false for OFF, true for ON

// Callback function to handle incoming messages
void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Null-terminate the payload
  String message = String((char*)payload);

  if (String(topic) == MQTT_TOPIC_KITCHEN_PUMP_CONTROL) {
    if (message == "ON") {
      digitalWrite(PUMP_KITCHEN, HIGH); // Turn the pump ON
      pumpState = true;
      Serial.println("Pump kitchen is ON");
    } else if (message == "OFF") {
      digitalWrite(PUMP_KITCHEN, LOW); // Turn the pump OFF
      pumpState = false;
      Serial.println("Pump kitchen is OFF");
    }
    // Publish the new state
    String stateMessage = pumpState ? "ON" : "OFF";
    client.publish(MQTT_TOPIC_KITCHEN_PUMP_STATE, stateMessage.c_str());
  }
  else if (String(topic) == MQTT_TOPIC_BEDROOM_PUMP_CONTROL) {
    if (message == "ON") {
      digitalWrite(PUMP_BEDROOM, HIGH); // Turn the pump ON
      pumpState = true;
      Serial.println("Pump bedroom is ON");
    } else if (message == "OFF") {
      digitalWrite(PUMP_BEDROOM, LOW); // Turn the pump OFF
      pumpState = false;
      Serial.println("Pump bedroom is OFF");
    }
    // Publish the new state
    String stateMessage = pumpState ? "ON" : "OFF";
    client.publish(MQTT_TOPIC_BEDROOM_PUMP_STATE, stateMessage.c_str());
  }
}

void setup() {
  Serial.begin(115200);    

  pinMode(PUMP_KITCHEN, OUTPUT); // Set the pump pin as an output
  digitalWrite(PUMP_KITCHEN, HIGH); // Initially turn the pump ON

  pinMode(PUMP_BEDROOM, OUTPUT); // Set the pump pin as an output
  digitalWrite(PUMP_BEDROOM, HIGH); // Initially turn the pump ON

  WiFi.begin(SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  client.setServer(MQTT_SERVER, 1883);
  client.setCallback(callback); // Set the callback function for incoming messages

  reconnectMQTT(); // Connect to MQTT
}

void reconnectWiFi() {
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
  if (WiFi.status() != WL_CONNECTED){
    return;  //will try to reconnect to wifi 
  }
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("pump_switch", USER, USER_PASSWORD)) {
      Serial.println("connected");
      client.subscribe(MQTT_TOPIC_KITCHEN_PUMP_CONTROL); // Subscribe to control topic
      client.subscribe(MQTT_TOPIC_BEDROOM_PUMP_CONTROL); 
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      delay(5000);
    }
  }
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    reconnectWiFi(); // Check and reconnect WiFi if needed
  }

  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}
