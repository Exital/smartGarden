/*********
  Smart garden esp32 sensors unit
  Technion IOT project
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>
#include <Servo.h>


// Board id
const char* board_id = "1";

Servo servo_valve;

// bmp instance to read bmp values
Adafruit_BMP280 bmp;

// Enum for valve state
enum ValveState {CLOSED = 0, OPEN = 1};

// Conversion factor for micro seconds to seconds
const uint64_t uS_TO_S_FACTOR = 1000000;

int take_calibration_measure(int pin, int num_of_samples=10);



// ------------------------ default values -----------------

// Lowest soil moisture percentage that plants need to thrive
const int lower_irigation_bound = 30;

// Highest soil moisture percentage that plants need to thrive
const int upper_irigation_bound = 50;

// Sleep time for ESP32 (in seconds)
RTC_DATA_ATTR uint64_t TIME_TO_SLEEP = 60;

// Timer for ESP32 stand alone server (in seconds) before returning to sleep cycle
const int TIME_FOR_STAND_ALONE_SERVER = 600;

//Servo valve state(savd to rtc data attr to kepp the vlues while sleeping)
RTC_DATA_ATTR ValveState valve_state = CLOSED;

//Calibration_mode
bool CALIBRATION_MODE = false;

// -------------------------------------------------------------


// ------------------------ default sensors values -----------------

// Global moisture percentage value(savd to rtc data attr to keep the vlues while sleeping)
RTC_DATA_ATTR int soil_moisture_value = -1;

// Global photoresistor percentage value(savd to rtc data attr to keep the values while sleeping)
RTC_DATA_ATTR int photoresistor_value = -1;

// Global battery percentage value(savd to rtc data attr to keep the values while sleeping)
RTC_DATA_ATTR int battery_percentage = -1;

// Global temprature percentage value(savd to rtc data attr to keep the values while sleeping)
RTC_DATA_ATTR int temprature_value = -100;

// Global boolean parameter of water level(savd to rtc data attr to keep the values while sleeping)
RTC_DATA_ATTR bool low_water_level = false;

//Moisture values(savd to rtc data attr to keep the values while sleeping)
RTC_DATA_ATTR int dry_soil_moisture_value = 4095;
RTC_DATA_ATTR int wet_soil_moisture_value = 1650;

//Photoresistor values(savd to rtc data attr to keep the values while sleeping)
RTC_DATA_ATTR int dark_value = 4095;
RTC_DATA_ATTR int light_value = 1850;

// -------------------------------------------------------------


// ------------------------ network info -----------------

// Global vars
WiFiClient espClient;
PubSubClient client(espClient);

// Pi network info:
const char* ssid = "SmartGarden";
const char* password = "technioniot";

//ESP32 network info:
const char* esp32_ssid     = "server";

// MQTT Broker IP address:
const char* mqtt_server = "192.168.50.10";

// topics to subscribe
const int subscriptions = 6;
char *subscribe_to[subscriptions] = {"led", "irigation", "sleep", "calibration", "soil_moisture_calibration", "light_calibration"};

// -------------------------------------------------------------


// ------------------------ analog pins -----------------

// Servo Pin
const int servo_pin = 26;

//Water level Pin
const int water_level_pin = 32;

//Button Pin GPIO_NUM_33

//Soil moisture Pin
const int soil_moisture_pin = 34;

//Photoresistor Pin
const int photoresistor_pin = 35;

//Battery monitoring pin
const int battery_monitor_pin = 39;

// -------------------------------------------------------------





// ------------------------ stand alone server -----------------
// Set web server port number to 80
WiFiServer server(80);

//Set stand alone mode to false(savd to rtc data attr to kepp the vlues while sleeping)
RTC_DATA_ATTR bool stand_alone_mode = false;

//Set stand alone timeout to false
bool stand_alone_server_timeout = false;

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
bool stand_alone_irigation_button = false;

// -------------------------------------------------------------

//Handles wakeup reason of ESP32 after waking up
void handle_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : handle_external_wake_up(); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

// Set stand alone server tiomeout to true
void IRAM_ATTR onTimer(){
  stand_alone_server_timeout = true;
  Serial.println("Stand alone server timeout!");
}


//Handles wakeup of ESP32 caused by external reason
void handle_external_wake_up(){
  if (stand_alone_mode){
    stand_alone_server_set_up();

  // Use 1st timer of 4
  // 1 tick take 1/(80MHZ/80) = 1us so we set divider 80 and count up
  hw_timer_t * timer = NULL;
  timer = timerBegin(0, 80, true);

  // Attach onTimer function to our timer
  timerAttachInterrupt(timer, &onTimer, true);

  // Set alarm to call onTimer function every second 1 tick is 1us
  //=> 1 second is 1000000us
  // Repeat the alarm (third parameter)
  timerAlarmWrite(timer, uS_TO_S_FACTOR * TIME_FOR_STAND_ALONE_SERVER, false);

  // Start an alarm
  timerAlarmEnable(timer);
  Serial.println("start timer");
    while(!stand_alone_server_timeout){
      stand_alone_server_loop();
    }
    Serial.println("going to sleep");
    ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  } else {
    calibration_mode();
  }
}

//Enter calibration mode after external wake up
void calibration_mode() {
  Serial.println("Wakeup caused by external signal using RTC_IO");
  Serial.println("Going in calibration mode");
  publish_msg("calibration", "start");
  CALIBRATION_MODE = true;
  while(CALIBRATION_MODE){
    client.loop();
  }
}


// Set up wifi
bool setup_wifi() {
  delay(10);

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int retries = 10;
  for(int i = 0; i < retries; i++){
    delay(500);
    Serial.print(".");
    if (WiFi.status() == WL_CONNECTED){
      break;
    }
  }


  if (WiFi.status() != WL_CONNECTED){  // Pi Wifi not found
    Serial.println("");
    Serial.println("Pi Wifi not found - stand alone mode initiating...");
    return false;
  } else {// Pi Wifi found
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
  }
}

//Board subscription
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

//Generic function to publish a message
void publish_msg(char* local_topic, char* msg){
  char topic[256];
  const char *board = "board/";
  strcpy(topic,board);
  strcat(topic,board_id);
  strcat(topic,"/");
  strcat(topic,local_topic);
  client.publish(topic, msg);
  Serial.print("Publishing topic=");
  Serial.print(topic);
  Serial.print(" msg=");
  Serial.println(msg);
  for(int i = 0; i < 20; i++){
  client.loop(); //Ensure we've sent & received everything
  }
}

// Sends sleep time
void send_sleep_time(){
  int duration = TIME_TO_SLEEP;
  String duration_str = String(duration);
  int str_len = duration_str.length() + 1;
  char char_array[str_len];
  duration_str.toCharArray(char_array, str_len);

  publish_msg("sleepTime", char_array);
}


//Sets a new sleep time duation received by user
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


// Mesures caibration valuse
int take_calibration_measure(int pin, int num_of_samples){
  delay(1000);
  int result = 0;
  for(int i = 0; i < num_of_samples; i++){
    result += analogRead(pin);
    delay(100);
  }
  return result / num_of_samples;
}

// Handles soil moisture sensor calibration
void handle_soil_moisture_calibration(String msg){
  if (msg == "wet"){ // read wet value
    wet_soil_moisture_value = take_calibration_measure(soil_moisture_pin);
    Serial.print("wet soil moisutre value=");
    Serial.println(wet_soil_moisture_value);
    publish_msg("soil_moisture_calibration", "wetOK");
  }
  if(msg == "dry"){ // read dry value
    dry_soil_moisture_value = take_calibration_measure(soil_moisture_pin);
    Serial.print("dry soil moisutre value=");
    Serial.println(dry_soil_moisture_value);
    publish_msg("soil_moisture_calibration", "dryOK");
  }
}

// Handles photoreistor sensor calibration
void handle_light_calibration(String msg){
  if (msg == "dark"){// read dark value
    dark_value = take_calibration_measure(photoresistor_pin);
    Serial.print("light sensor dark value=");
    Serial.println(dark_value);
    publish_msg("light_calibration", "darkOK");
  }
  if(msg == "light"){// read ligh value
    light_value = take_calibration_measure(photoresistor_pin);
    Serial.print("light sensor light value=");
    Serial.println(light_value);
    publish_msg("light_calibration", "lightOK");
  }
}

//Sets calibratio mode to false
void handle_calibration_off(){
  Serial.println("Calibration mode off");
  CALIBRATION_MODE = false;
}

//Help function to map float values
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// Handles battery level
void handle_battery_level(){
  int monitor_value = analogRead(battery_monitor_pin);
  float calibration = 0.3;
  float voltage = (((monitor_value * 3.3) / 4096 ) * 2) + calibration;
  Serial.print("voltage=");
  Serial.println(voltage);
  battery_percentage = mapfloat(voltage, 3.2, 4.1, 0, 100);

  if(battery_percentage >= 100) battery_percentage = 100;
  if(battery_percentage < 0) battery_percentage = 0;

  Serial.print("Battery percentage=");
  Serial.println(battery_percentage);

  // Adjust string for publish messege
  String battery_percentage_str = String(battery_percentage);
  int str_len = battery_percentage_str.length() + 1;
  char char_array[str_len];
  battery_percentage_str.toCharArray(char_array, str_len);

  //publish to mqtt server
  publish_msg("battery_percentage", char_array);
}

// Handles soil moisture sensor
void handle_soil_moisture_sensor(){
  int soil_moisture = analogRead(soil_moisture_pin);
  int soil_moisture_percentage = map(soil_moisture, wet_soil_moisture_value, dry_soil_moisture_value, 100, 0);

  if(soil_moisture_percentage < 0){
    soil_moisture_percentage = 0;
  }
  if(soil_moisture_percentage > 100){
    soil_moisture_percentage = 100;
  }

  // Adjust string for publish messege
  String soil_moisture_percentage_str = String(soil_moisture_percentage);
  int str_len = soil_moisture_percentage_str.length() + 1;
  char char_array[str_len];
  soil_moisture_percentage_str.toCharArray(char_array, str_len);

  //publish to mqtt server
  publish_msg("soil_moisture", char_array);
  soil_moisture_value = soil_moisture_percentage;
}


// Handles water level
void handle_water_height(){
  int water_height = analogRead(water_level_pin);
  int water_height_mid = (wet_soil_moisture_value + dry_soil_moisture_value) / 2;
  if(water_height > water_height_mid){
  low_water_level = true;
  publish_msg("water_level", "low");
  } else {
    low_water_level = false;
    publish_msg("water_level", "high");
  }
}

// Handles photoresistor sensor
void handle_photoresistor_sensor(){
  int photoresistor = analogRead(photoresistor_pin);

  //Serial.println(photoresistor);// DEBUG
  int photoresistor_percentage = map(photoresistor, light_value, dark_value, 100, 0);

  //Serial.print(photoresistor_percentage);// DEBUG
  //Serial.println("%");// DEBUG
  if(photoresistor_percentage < 0){
    photoresistor_percentage = 0;
  }
  if(photoresistor_percentage > 100){
    photoresistor_percentage = 100;
  }

  // Adjust string for publish messege
  String photoresistor_percentage_str = String(photoresistor_percentage);
  int str_len = photoresistor_percentage_str.length() + 1;
  char char_array[str_len];
  photoresistor_percentage_str.toCharArray(char_array, str_len);

  //publish to mqtt server
  publish_msg("photoresistor", char_array);
  photoresistor_value = photoresistor_percentage;
}

// Handles temprature sensor
void handle_temprature_sensor(){

  float temprature = bmp.readTemperature();
  //Serial.print(temprature);// DEBUG

  // Adjust string for publish messege
  String temprature_str = String(temprature);
  int str_len = temprature_str.length() + 1;
  char char_array[str_len];
  temprature_str.toCharArray(char_array, str_len);

  //publish to mqtt server
  publish_msg("temprature", char_array);
  temprature_value = temprature;
}

// Handles manual irrigation
void handle_irigation(){
  if(!low_water_level){
    if (valve_state == CLOSED){
      Serial.println("manual irigation started");
      open_close_valve();
      Serial.println("manual irigation ended");
    }
    publish_msg("irigationOK", "true");
  } else {
      publish_msg("irigationOK", "false");
  }
}

// Handles auto irrigation
void auto_irigation(){
  if(!low_water_level){
    if (valve_state == CLOSED){
      if (soil_moisture_value < lower_irigation_bound) open_valve();
    } else {
      if (soil_moisture_value > upper_irigation_bound) close_valve();
      }
    } else {
        if (valve_state == OPEN) close_valve();
  }
}

//Cals all sensor handlers
void call_sensors_handlers(){
  handle_soil_moisture_sensor();
  handle_photoresistor_sensor();
  handle_battery_level();
  handle_temprature_sensor();
  handle_water_height();
  auto_irigation();
}

// Open irrigation valve
void open_valve(){
  if(valve_state == CLOSED){
    int valve_close_angle = 0;
    int valve_open_angle = 180;
    Serial.println("Servo valve opening");
  
    for(int angle = valve_close_angle; angle <= valve_open_angle; angle +=1) {
      servo_valve.write(angle);
      delay(20);
    }
  }
  valve_state = OPEN;
}

//Close irrigation valve
void close_valve(){
  if(valve_state == OPEN){
    int valve_close_angle = 0;
    int valve_open_angle = 180;
    Serial.println("Servo valve closing");
  
    for(int angle = valve_open_angle; angle >= valve_close_angle; angle -=1) {
      servo_valve.write(angle);
      delay(20);
    }
    valve_state = CLOSED;
  }
}

//Helper for manual irrigation
void open_close_valve(){
  open_valve();
  delay(2000);
  close_valve();
}

//Handles connection timeout
void handle_timeout_comOK(){
  send_sleep_time();
  delay(200);
  publish_msg("comOK", "true");
}

// this is the entry point of a received topic and message for board_id
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
  if (local_topic == "irigation" && messageTemp == "true") handle_irigation();
  if (local_topic == "sleep") handle_sleep_duration_change(messageTemp);
  if (local_topic == "calibration" && messageTemp == "end") handle_calibration_off();
  if (local_topic == "soil_moisture_calibration") handle_soil_moisture_calibration(messageTemp);
  if (local_topic == "light_calibration") handle_light_calibration(messageTemp);
}


// Reconnecting to server
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(board_id)) {
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

// Loop function - empty since the ESP32 never gets here because of the deepSleep cycle
void loop() {

}

// Set up for stand alone
void stand_alone_server_set_up(){
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Setting AP (Access Point)…");
  char server_name[256];
  const char *board = "Board_";
  strcpy(server_name,board);
  strcat(server_name,board_id);
  strcat(server_name,"_Server");
  WiFi.softAP(server_name, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  server.begin();
}

// Creates the stand alone html sensor values table
void create_html_data_table(WiFiClient* server){

  call_sensors_handlers();

  //create the table
  server->println("<h2>נתוני חיישנים</h2>");
  server->println("<table class=\"center\"><tr><th>חיישן</th><th>ערך</th></tr>");
  server->println("<tr><td>סוללה</td><td>");
  server->println(battery_percentage);
  server->println("%</td></tr>");
  server->println("<tr><td>לחות</td><td>");
  server->println(soil_moisture_value);
  server->println("%</td></tr>");
  server->println("<tr><td>טמפרטורה</td><td>");
  server->println(temprature_value);
  server->println("C</td></tr>");
  server->println("<tr><td>נדרש מילוי מיכל מים</td><td>");
  if(low_water_level){
    server->println("כן");
  } else {
    server->println("לא");
  }
  server->println("</td></tr>");
  server->println("<tr><td>אור</td><td>");
  server->println(photoresistor_value);
  server->println("%</td></tr></table>");


}

// Stand alone server handler
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
            client.println("Connection: keep-alive");
            client.println();

            // turns the GPIOs on and off
            if (header.indexOf("GET /irigation/on") >= 0) {
              Serial.println("irigation button clicked - ON");
              handle_irigation();
              stand_alone_irigation_button = true;
            } else if (header.indexOf("GET /irigation/off") >= 0) {
              Serial.println("irigation button clicked - OFF");
              stand_alone_irigation_button = false;
            }

            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<html lang=\"he\" dir=\"rtl\">");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<meta http-equiv='refresh' content=\"10; url=http://192.168.4.1\"/>");
            client.println("<meta charset=\"utf-8\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #4CAF50; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #555555;}");
            client.println("table {font-family: arial, sans-serif; text-align: center; border-collapse: collapse; border: 1px solid #ddd; width: 60%;}");
            client.println("td, th {border: 1px solid #ddd; text-align: center; padding: 15px;}");
            client.println("table.center {margin-left: auto; margin-right: auto;}</style></head>");


            // Web Page Heading
            client.println("<body><h1>Stand Alone Server</h1>");

            // Display irigation button
            if (!stand_alone_irigation_button) {
              client.println("<p><a href=\"/irigation/on\"><button class=\"button\">הפעל השקיה</button></a></p>");
            } else {
              stand_alone_irigation_button = false;
              client.println("<p><a href=\"/irigation/off\"><button class=\"button button2\">פקודה נשלחה</button></a></p>");
            }

			//Create the html table
            create_html_data_table(&client);
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


// System setup
void setup() {
  // set serial port to be 115200 baud rate
  Serial.begin(115200);

  //attach servo pin
  servo_valve.attach(servo_pin);

  // set bmp(temprature) sensor (0x76=default i2c address)
  bmp.begin(0x76);

  //set stand alone mode
  stand_alone_mode = !setup_wifi();

  //set mqtt server
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println("woke up");
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_33,1); //1 = High, 0 = Low

  if (!client.connected() && !stand_alone_mode) { // in case no client connected and we are
												  //not in stand alone mode try to reconnect
      reconnect();
  }

  handle_wakeup_reason();
  call_sensors_handlers();

  for(int i = 0; i < 20; i++){
  client.loop(); //Ensure we've sent & received everything
  delay(100);
  }
  handle_timeout_comOK();
  Serial.println("going to sleep");
  ESP.deepSleep(TIME_TO_SLEEP * uS_TO_S_FACTOR);
}
