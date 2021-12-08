/*********
  Smart garden esp32 sensors unit
  Technion IOT project
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Servo_ESP32.h>

static const int servoPin = 14; //printed G14 on the board

Servo_ESP32 servo1;

int angle =0;
int angleStep = 1;

int angleMin =0;
int angleMax = 180;

uint64_t uS_TO_S_FACTOR = 1000000;  /* Conversion factor for micro seconds to seconds */
RTC_DATA_ATTR uint64_t TIME_TO_SLEEP = 60;        /* Time ESP32 will go to sleep (in seconds) */

int take_calibration_measure(int pin, int num_of_samples=10);

// Pi network info:
const char* ssid = "SmartGarden";
const char* password = "technioniot";

//ESP32 network info:
const char* esp32_ssid     = "ESP32-Access-Point";
const char* esp32_password = "123456789";

// MQTT Broker IP address:
const char* mqtt_server = "192.168.50.10";

// Servo Pin
const int servo_pin = 26;

//Soil moisture Pin (analog pin)
const int soil_moisture_pin = 34;

//Photoresistor Pin (analog pin)
const int photoresistor_pin = 35;

// Board id
const char* board_id = "1";

// topics to subscribe
const int subscriptions = 5;
char *subscribe_to[subscriptions] = {"led", "irigation", "sleep", "calibration", "soil_moisture_calibration"};

//Moisture values
RTC_DATA_ATTR int dry_soil_moisture_value = 2500;
RTC_DATA_ATTR int wet_soil_moisture_value = 1200;

//Photoresistor values
RTC_DATA_ATTR int low_value = 3000;
RTC_DATA_ATTR int high_value = 500;

//Calibration_mode
bool CALIBRATION_MODE = false;

WiFiClient espClient;
PubSubClient client(espClient);

// ------------------------ stand alone server -----------------
// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output26State = "off";
String output27State = "off";

// Assign output variables to GPIO pins
const int output26 = 26;
const int output27 = 27;

// -------------------------------------------------------------

void handle_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : calibration_mode(); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

void calibration_mode() {
  Serial.println("Wakeup caused by external signal using RTC_IO");
  Serial.println("Going in calibration mode");
  publish_msg("calibration", "start");
  CALIBRATION_MODE = true;
  while(CALIBRATION_MODE){
    client.loop();
  }
}


bool setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int retries = 30;
  for(int i = 0; i < retries; i++){
    delay(500);
    Serial.print(".");
    if (WiFi.status() == WL_CONNECTED){
      break;
    }
  }

  if (WiFi.status() != WL_CONNECTED){
    Serial.println("Pi Wifi not found - stand alone mode initiating...");
    return false;
  } else {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
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

int take_calibration_measure(int pin, int num_of_samples){
  delay(100);
  int result = 0;
  for(int i = 0; i < num_of_samples; i++){
    result += analogRead(pin);
    delay(100);
  }
  return result / num_of_samples;
}

void handle_soil_moisture_calibration(String msg){
  if (msg == "wet"){
    wet_soil_moisture_value = take_calibration_measure(soil_moisture_pin);
    Serial.print("wet soil moisutre value=");
    Serial.println(wet_soil_moisture_value);
    publish_msg("soil_moisture_calibration", "wetOK");
  }
  if(msg == "dry"){
    dry_soil_moisture_value = take_calibration_measure(soil_moisture_pin);
    Serial.print("dry soil moisutre value=");
    Serial.println(dry_soil_moisture_value);
    publish_msg("soil_moisture_calibration", "dryOK");
  }
}

void handle_calibration_off(){
  Serial.println("Calibration mode off");
  CALIBRATION_MODE = false;
}


void handle_soil_moisture_sensor(){
  int soil_moisture = analogRead(soil_moisture_pin);
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
    servo_move();
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
  if (local_topic == "irigation") handle_irigation(messageTemp);
  if (local_topic == "sleep") handle_sleep_duration_change(messageTemp);
  if (local_topic == "calibration" && messageTemp == "end") handle_calibration_off();
  if (local_topic == "soil_moisture_calibration") handle_soil_moisture_calibration(messageTemp);

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

void servo_move(){
    for(int angle = 0; angle <= angleMax; angle +=angleStep) {
      servo1.write(angle);
      delay(20);
  }
  delay(2000);

  for(int angle = 180; angle >= angleMin; angle -=angleStep) {
      servo1.write(angle);
      delay(20);
  }
}

void loop() {

}

void stand_alone_server_set_up(){
  // Initialize the output variables as outputs
  pinMode(output26, OUTPUT);
  pinMode(output27, OUTPUT);
  // Set outputs to LOW
  digitalWrite(output26, LOW);
  digitalWrite(output27, LOW);

  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  // Remove the password parameter, if you want the AP (Access Point) to be open
  WiFi.softAP(esp32_ssid, esp32_password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);
  
  server.begin();
}

void stand_alone_server_loop(){
  WiFiClient client = server.available();   // Listen for incoming clients

  if (client) {                             // If a new client connects,
    Serial.println("New Client.");          // print a message out in the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        header += c;
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // turns the GPIOs on and off
            if (header.indexOf("GET /26/on") >= 0) {
              Serial.println("GPIO 26 on");
              output26State = "on";
              digitalWrite(output26, HIGH);
            } else if (header.indexOf("GET /26/off") >= 0) {
              Serial.println("GPIO 26 off");
              output26State = "off";
              digitalWrite(output26, LOW);
            } else if (header.indexOf("GET /27/on") >= 0) {
              Serial.println("GPIO 27 on");
              output27State = "on";
              digitalWrite(output27, HIGH);
            } else if (header.indexOf("GET /27/off") >= 0) {
              Serial.println("GPIO 27 off");
              output27State = "off";
              digitalWrite(output27, LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>ESP32 Web Server</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 26  
            client.println("<p>GPIO 26 - State " + output26State + "</p>");
            // If the output26State is off, it displays the ON button       
            if (output26State=="off") {
              client.println("<p><a href=\"/26/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/26/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               
            // Display current state, and ON/OFF buttons for GPIO 27  
            client.println("<p>GPIO 27 - State " + output27State + "</p>");
            // If the output27State is off, it displays the ON button       
            if (output27State=="off") {
              client.println("<p><a href=\"/27/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/27/off\"><button class=\"button button2\">OFF</button></a></p>");
            }
            client.println("</body></html>");
            
            // The HTTP response ends with another blank line
            client.println();
            // Break out of the while loop
            break;
          } else { // if you got a newline, then clear currentLine
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }
    }
    // Clear the header variable
    header = "";
    // Close the connection
    client.stop();
    Serial.println("Client disconnected.");
    Serial.println("");
  }
}

void setup() {
  Serial.begin(115200);
  servo1.attach(servo_pin);
  bool stand_alone = !setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(servo_pin, OUTPUT);
  pinMode(soil_moisture_pin, INPUT);
  pinMode(photoresistor_pin, INPUT);
  Serial.println("woke up");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low
  if (stand_alone){
    stand_alone_server_set_up();
    while(true){
      stand_alone_server_loop();
    }
  } else if (!client.connected()) {
      reconnect();
  }

  handle_wakeup_reason();
  handle_soil_moisture_sensor();
  handle_photoresistor_sensor();

  for(int i = 0; i < 10; i++){
  client.loop(); //Ensure we've sent & received everything
  delay(100);
  }
  Serial.println("going to sleep");
  ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}
