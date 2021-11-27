/*********
  Smart garden esp32 sensors unit
  Technion IOT project
*********/

uint64_t uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR uint64_t TIME_TO_SLEEP = 10;        /* Time ESP32 will go to sleep (in seconds) */


#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

//RTC_DATA_ATTR int recordCounter = 0; RTC_DATA_ATTR get saved in rtc memory between sleep cycles!!

// Pi network info:
const char* ssid = "SmartGarden";
const char* password = "technioniot";

// MQTT Broker IP address:
const char* mqtt_server = "192.168.50.10";

// LED Pin
const int ledPin = 26;

//Soil moisture Pin (analog pin)
const int siol_moisture_pin = 34;

//Photoresistor Pin (analog pin)
const int photoresistor_pin = 35;

// Board id
const char* board_id = "1";

// topics to subscribe
const int subscriptions = 3;
char *subscribe_to[subscriptions] = {"led", "irigation", "sleep"};

//Moisture values
const int dry_soil_moisture_value = 2500;
const int wet_soil_moisture_value = 1200;

//Photoresistor values
const int low_value = 3000;
const int high_value = 500;

WiFiClient espClient;
PubSubClient client(espClient);


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

void handle_sleep_duration_change(String msg){
  if (msg != ""){
    Serial.print("Sleep duration set to ");
    Serial.println(msg);
    int duration = msg.toInt();
    TIME_TO_SLEEP = duration;
    
    int str_len = msg.length() + 1; 
    char char_array[str_len];
    msg.toCharArray(char_array, str_len);
  
    publish_msg("sleepOK", char_array);
  }
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

void handle_soil_moisture_sensor(){
  int soil_moisture = analogRead(siol_moisture_pin);
  int soil_moisture_percentage = map(soil_moisture, wet_soil_moisture_value, dry_soil_moisture_value, 100, 0);
  
  //Serial.print(soil_moisture_percentage);// DEBUG
  //Serial.println("%");// DEBUG
  
  String soil_moisture_percentage_str = String(soil_moisture_percentage);
  int str_len = soil_moisture_percentage_str.length() + 1; 
  char char_array[str_len];
  soil_moisture_percentage_str.toCharArray(char_array, str_len);
  
  //publish to mqtt server
  publish_msg("soil_moisture", char_array);

}

void handle_photoresistor_sensor(){
  int photoresistor = analogRead(photoresistor_pin);
  
  //Serial.println(photoresistor);// DEBUG
  int photoresistor_percentage = map(photoresistor, high_value, low_value, 100, 0);
  
  //Serial.print(soil_moisture_percentage);// DEBUG
  //Serial.println("%");// DEBUG
  
  String photoresistor_percentage_str = String(photoresistor_percentage);
  int str_len = photoresistor_percentage_str.length() + 1; 
  char char_array[str_len];
  photoresistor_percentage_str.toCharArray(char_array, str_len);
  
  //publish to mqtt server
  publish_msg("photoresistor", char_array);

}

void handle_irigation(String msg){
  if(msg == "true"){
    Serial.println("irigation started");
    digitalWrite(ledPin, HIGH);
    delay(3000);
    digitalWrite(ledPin, LOW);
    publish_msg("irigationOK", "false");
    Serial.println("irigation ended");
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
  if (local_topic == "irigation") handle_irigation(messageTemp);
  if (local_topic == "sleep") handle_sleep_duration_change(messageTemp);
  
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
  
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(ledPin, OUTPUT);
  pinMode(siol_moisture_pin, INPUT);
  pinMode(siol_moisture_pin, INPUT);
  Serial.println("woke up");

  if (!client.connected()) {
    reconnect();
  }
  handle_soil_moisture_sensor();
  handle_photoresistor_sensor();

  for(int i = 0; i < 10; i++){
  client.loop(); //Ensure we've sent & received everything
  delay(100);
  }
  Serial.println("going to sleep");
  ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}
