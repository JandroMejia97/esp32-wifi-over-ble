#include <WiFi.h>
#include <PubSubClient.h>

// WiFi
const char *ssid = "Fibertel WiFi839 2.4Ghz"; // Enter your WiFi name
const char *password = "00496026574";  // Enter WiFi password

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *server_topic = "server/data";
const char *esp32_topic = "esp32/data";
const char *mqtt_username = "ciaa_tdp";
const char *mqtt_password = "c144_tdp";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
 // Set software serial baud to 115200;
 Serial.begin(115200);
 // connecting to a WiFi network
 WiFi.begin(ssid, password);
 while (WiFi.status() != WL_CONNECTED) {
     delay(500);
     Serial.println("Connecting to WiFi..");
 }
 Serial.println("Connected to the WiFi network");
 //connecting to a mqtt broker
 client.setServer(mqtt_broker, mqtt_port);
 client.setCallback(callback);
 while (!client.connected()) {
     String client_id = "mqttx_2b608a4e";
     client_id += String(WiFi.macAddress());
     Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
     if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
         Serial.println("Public emqx mqtt broker connected.");
     } else {
         Serial.print("Failed with state ");
         Serial.print(client.state());
         Serial.println();
         Serial.println("-----------------------");
         delay(2000);
     }
 }
 // publish and subscribe
 client.publish(esp32_topic, "{\"msg\": \"ESP32 Connected\"}");
 client.subscribe(server_topic);
}

void callback(char *topic, byte *payload, unsigned int length) {
 Serial.print("Message arrived in topic: ");
 Serial.println(topic);
 int i;
 char data[length];
 for (i = 0; i < length; i++) {
     data[i] = (char) payload[i];
 }
 data[i] = '\0';
 Serial.printf("Data: \n%s\n", data);
 client.publish(esp32_topic, data);
 Serial.println();
 Serial.println("-----------------------");
}

void loop() {
 client.loop();
}