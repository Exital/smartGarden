/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com  
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// Pi network info:
const char* ssid = "SmartGarden";
const char* password = "technioniot";

// MQTT Broker IP address:
const char* mqtt_server = "192.168.50.10";

// LED Pin
const int ledPin = 26;

// Board id
const char* board_id = "1";

// topics to subscribe
const int subscriptions = 1;
char *subscribe_to[subscriptions] = {"led"};

WiFiClient espClient;
PubSubClient client(espClient);



void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(ledPin, OUTPUT);
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void make_subscriptions(){
    for (int i = 0; i < subscriptions; i++) {
      char topic[256];
      const char *board = "board/";
      strcpy(topic,board);
      strcat(topic,board_id);
      strcat(topic,"/");
      strcat(topic, subscribe_to[i]);
      Serial.print("Subscribing to ");
      Serial.println(topic);
      client.subscribe(topic);
  }
}

void publish_msg(char* local_topic, char* msg){
  char topic[256];
  const char *board = "board/";
  strcpy(topic,board);
  strcat(topic,board_id);
  strcat(topic,"/");
  strcat(topic,local_topic);
  client.publish(topic, msg);
}

void handle_led(String msg){
  Serial.print("Changing output to ");
  Serial.println(msg);
  if(msg == "true"){
    digitalWrite(ledPin, HIGH);
  }
  else if(msg == "false"){
    digitalWrite(ledPin, LOW);
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  // print what topic and message have arrived to serial
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // removing board/<id>/ from topic
  topic = &topic[8];
  String local_topic = String(topic);
  if (local_topic == "led") handle_led(messageTemp);
  
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP32Client")) {
      Serial.println("connected");
      // Subscribe
      make_subscriptions();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
}
