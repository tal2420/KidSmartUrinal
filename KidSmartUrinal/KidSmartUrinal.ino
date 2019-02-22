#include <WiFiClient.h>
#include <WiFiServer.h>
#include <WiFiUdp.h>

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <timer.h>
#include <PubSubClient.h>

// Replace with your network credentials
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

const int ESP_BUILTIN_LED = 2;
const int LIGHT_LED = 14;
const int FLUSH_RELAY = 12;
const int FRESH_RELAY = 16;
const int BUTTON_LED = 3;

#define TRIGGER 4
#define ECHO 0

int ledState = LOW;
int buttonLedState = LOW;
int freshState = HIGH;
int flushState = HIGH;
int ledLight = HIGH;

int isConnect = LOW;
int isNear = LOW;
long nearDuration = 0;

int isCycleRun = HIGH;

unsigned long proximityCm = 30;
unsigned long turnOffLightMillis = 0;

unsigned long turnOffFlush1Millis = 0;
unsigned long turnOnFreshMillis = 0;
unsigned long turnOffFreshMillis = 0;
unsigned long turnOnFlush2Millis = 0;
unsigned long turnOffFlush2Millis = 0;

const long interval = 1000;           // interval at which to blink (milliseconds)

long intervalFlush1Off = 30000;
long intervalFreshOn = intervalFlush1Off + 1000; 
long intervalFreshOff = intervalFreshOn + 5000;

long intervalFlush2On = intervalFreshOff + 1000;
long intervalFlush2Off = intervalFlush2On + 20000;

const long intervalLight = 15000; 

const int ledPin =  LED_BUILTIN;

const int buttonPin = 15;
// variables will change:
int buttonState = 0;  
int buttonChange = 0;

auto timer = timer_create_default(); // create a timer with default settings
auto timer1 = timer_create_default();

// Webserver logic
/*
// Set web server port number to 80
WiFiServer server(80);

// Variable to store the HTTP request
String header;

// Auxiliar variables to store the current output state
String output5State = "off";
String output4State = "off";

// Assign output variables to GPIO pins
const int output5 = FLUSH_RELAY;
const int output4 = FRESH_RELAY;
*/
// End webservewr logic

bool toggle_btn_led(void *) {
  // if flush in process - blink
  if (isCycleRun == LOW) {
      Serial.println("dbg_flush");
      if (buttonLedState == LOW) {
        buttonLedState = HIGH;
      } else {
        buttonLedState = LOW;
    }
    digitalWrite(BUTTON_LED, buttonLedState);
  }

  else {
    digitalWrite(BUTTON_LED, ledLight);
  }
  
}

bool toggle_led(void *) {
  if (isConnect == HIGH) {
    ledState = LOW;
  }
  else {
       // if the LED is off turn it on and vice-versa:
      if (ledState == LOW) {
        ledState = HIGH;
      } else {
        ledState = LOW;
    }
  }
    // set the LED with the ledState of the variable:
    digitalWrite(ledPin, ledState);
    

   if (buttonState == LOW && buttonChange == 1) {
      buttonChange = 0;
    }
    
    // Distance Sensor
    long duration, distance;
    digitalWrite(TRIGGER, LOW);  
    delayMicroseconds(2); 
    
    digitalWrite(TRIGGER, HIGH);
    delayMicroseconds(10); 
    
    digitalWrite(TRIGGER, LOW);
    duration = pulseIn(ECHO, HIGH);
    distance = (duration/2) / 29.1;

    if (distance < proximityCm && distance > 2) {
      isNear = HIGH;
      nearDuration = nearDuration +1;
      // turn on the light only if detected proximity for 2 seconds
      if (nearDuration >= 2) {
        digitalWrite(LIGHT_LED, HIGH);
        ledLight = HIGH;
        turnOffLightMillis = millis() + intervalLight;
      }
    }
    else {
      isNear = LOW;
      nearDuration = 0;
    }
    
    Serial.print(distance);
    Serial.println("Centimeter:");
    // End Distance Sensor
    //Serial.println("Check light");
    return true; // keep timer active? true

}



void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(LIGHT_LED, OUTPUT);
  pinMode(BUTTON_LED, OUTPUT);
  pinMode(BUILTIN_LED, OUTPUT);
  pinMode(FLUSH_RELAY,OUTPUT);
  pinMode(FRESH_RELAY,OUTPUT);
  pinMode(buttonPin, INPUT);

  digitalWrite(FLUSH_RELAY, flushState);
  digitalWrite(FRESH_RELAY, freshState);
  
  Serial.println("Booting");
  WiFi.hostname("SmartUrinal");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  IPAddress ip(192,168,1,65);   
  IPAddress gateway(192,168,1,1);   
  IPAddress subnet(255,255,255,0);   
  WiFi.config(ip, gateway, subnet);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    isConnect = LOW;
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword((const char *)"123");

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
    isConnect = LOW;
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  isConnect = HIGH;
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  pinMode(ESP_BUILTIN_LED, OUTPUT);

  digitalWrite(FLUSH_RELAY, HIGH);
  digitalWrite(FRESH_RELAY, HIGH);
  
  //Webserver 
  //server.begin();
  
  timer.every(1000, toggle_led);
  timer1.every(1000, toggle_btn_led);
}

void loop() {
  ArduinoOTA.handle();
  unsigned long currentMillis = millis();

  buttonState = digitalRead(buttonPin);
  
  if (buttonState == HIGH && buttonChange == 0) {
    if (turnOffFlush1Millis > 0 || turnOnFreshMillis > 0 || turnOffFreshMillis > 0 || turnOnFlush2Millis > 0 || turnOffFlush2Millis > 0) {
      Serial.println("Ignoring button - already in progress");
    }
    else {
      Serial.println("Button");
      isCycleRun = LOW;
      digitalWrite(FLUSH_RELAY, LOW);
      flushState = LOW;
      buttonChange = 1;
      turnOffFlush1Millis =  millis() + intervalFlush1Off;
      turnOnFreshMillis =  millis() + intervalFreshOn;
      turnOffFreshMillis =  millis() + intervalFreshOff;
      turnOnFlush2Millis =  millis() + intervalFlush2On;
      turnOffFlush2Millis =  millis() + intervalFlush2Off;
      
    }
  }
  
  timer.tick(); // tick the timer
  timer1.tick(); // tick the timer

  
  // Check if it's time to turn off the light
  if (turnOffLightMillis > 0 && turnOffLightMillis <= currentMillis){
    //turn off light
    if (isCycleRun == HIGH) {
     if (ledLight == HIGH) {
        digitalWrite(LIGHT_LED, LOW);
        ledLight = LOW;
        turnOffLightMillis = 0;
      }
    }
  }

   // Check if it's time to turn off the flush
  if (turnOffFlush1Millis > 0 && turnOffFlush1Millis <= currentMillis){
    //turn off flush
     if (flushState == LOW) {
        digitalWrite(FLUSH_RELAY, HIGH);
        flushState = HIGH;
        turnOffFlush1Millis = 0;
    }
  }

   // Check if it's time to turn on the fresh water
  if (turnOnFreshMillis > 0 && turnOnFreshMillis <= currentMillis){
    //turn on fresh water
     if (freshState == HIGH) {
        digitalWrite(FRESH_RELAY, LOW);
        freshState = LOW;
        turnOnFreshMillis = 0;
    }
  }
  
     // Check if it's time to turn off the fresh water
  if (turnOffFreshMillis > 0 && turnOffFreshMillis <= currentMillis){
    //turn on fresh water
     if (freshState == LOW) {
        digitalWrite(FRESH_RELAY, HIGH);
        freshState = HIGH;
        turnOffFreshMillis = 0;
    }
  }
  

  // Check if it's time to turn on the flush2 cycle
  if (turnOnFlush2Millis > 0 && turnOnFlush2Millis <= currentMillis){
    //turn on flush2 cycle
     if (flushState == HIGH) {
        digitalWrite(FLUSH_RELAY, LOW);
        flushState = LOW;
        turnOnFlush2Millis = 0;
    }
  }

   // Check if it's time to turn off the flush2 cycle
  if (turnOffFlush2Millis > 0 && turnOffFlush2Millis <= currentMillis){
    //turn off the flush2 cycle
     if (flushState == LOW) {
        digitalWrite(FLUSH_RELAY, HIGH);
        flushState = HIGH;
        turnOffFlush2Millis = 0;
        isCycleRun = HIGH;
    }
  }

  //Webserver logic
  /*
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
            if (header.indexOf("GET /5/on") >= 0) {
              Serial.println("GPIO 5 on");
              output5State = "on";
              digitalWrite(output5, HIGH);
            } else if (header.indexOf("GET /5/off") >= 0) {
              Serial.println("GPIO 5 off");
              output5State = "off";
              digitalWrite(output5, LOW);
            } else if (header.indexOf("GET /4/on") >= 0) {
              Serial.println("GPIO 4 on");
              output4State = "on";
              digitalWrite(output4, HIGH);
            } else if (header.indexOf("GET /4/off") >= 0) {
              Serial.println("GPIO 4 off");
              output4State = "off";
              digitalWrite(output4, LOW);
            }
            
            // Display the HTML web page
            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<link rel=\"icon\" href=\"data:,\">");
            // CSS to style the on/off buttons 
            // Feel free to change the background-color and font-size attributes to fit your preferences
            client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
            client.println(".button { background-color: #195B6A; border: none; color: white; padding: 16px 40px;");
            client.println("text-decoration: none; font-size: 30px; margin: 2px; cursor: pointer;}");
            client.println(".button2 {background-color: #77878A;}</style></head>");
            
            // Web Page Heading
            client.println("<body><h1>Smart Kid Urinal</h1>");
            
            // Display current state, and ON/OFF buttons for GPIO 5  
            client.println("<p>FLUSH - State " + output5State + "</p>");
            // If the output5State is off, it displays the ON button       
            if (output5State=="off") {
              client.println("<p><a href=\"/5/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/5/off\"><button class=\"button button2\">OFF</button></a></p>");
            } 
               
            // Display current state, and ON/OFF buttons for GPIO 4  
            client.println("<p>FRESH - State " + output4State + "</p>");
            // If the output4State is off, it displays the ON button       
            if (output4State=="off") {
              client.println("<p><a href=\"/4/on\"><button class=\"button\">ON</button></a></p>");
            } else {
              client.println("<p><a href=\"/4/off\"><button class=\"button button2\">OFF</button></a></p>");
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
  */
}