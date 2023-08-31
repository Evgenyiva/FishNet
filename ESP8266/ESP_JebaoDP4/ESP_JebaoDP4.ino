#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "Arduino.h"
#include <stdio.h>
#include <Shifty.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Declare the shift register
Shifty shift; 

// Set the clock, data, and latch pins you are using
// This also sets the pinMode for these pins
int SER_Pin = 14;   //pin D5 on the D1 mini DA 
int RCLK_Pin = 12;  //pin D6 on the D1 mini ST
int SRCLK_Pin = 13; //pin D7 on the D1 mini CH

// Update these with values suitable for your network.
const char* ssid = "GottaBounce";
const char* password = "Asdf1234";
const char* mqtt_server = "192.168.179.4";
const int mqtt_port = 1884;
const char* mqtt_user = "******";
const char* mqtt_password = "***";

unsigned long lastAliveTimestamp = 0;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
int myPin = 0;
// Set the number of bits you have (multiples of 8)
int shiftBits = 8;
String myTopic = "";
String myPayload = "";
char pin = '0';


void setup_shift() {
  shift.setBitCount(shiftBits);

  shift.setPins(SER_Pin, SRCLK_Pin, RCLK_Pin); 
  
  //Set onstart all bits to LOW
  for (int i = 0; i <= shiftBits; i++) {
    shift.writeBit(i, LOW);
  }
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  WiFi.hostname("ESP_JebaoDP4");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]");
  
  String pinStr = "0";
  pinStr = ((String)((char)topic[strlen(topic)-1]));
  int myPin = pinStr.toInt();
  Serial.println();

  Serial.println((char)payload[0]);
  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1' ) {
    shift.writeBit(myPin, HIGH);   
  } else {
    shift.writeBit(myPin, LOW); 
  }

  client.publish("Sensor/DP4", topic );
}

void publishShiftRegisterState() {
  String stateMessage = "";

  for (int i = 0; i < shiftBits; i++) {
    int bitValue = shift.readBit(i);
    stateMessage += String(bitValue);
  }

  // Publish the state to MQTT topic
  client.publish("dosing/DP4/state", stateMessage.c_str());
}

void publishAliveMessage() {
  char timestampBuffer[20];
  snprintf(timestampBuffer, sizeof(timestampBuffer), "%lu", lastAliveTimestamp);

  client.publish("dosing/DP4/alive", timestampBuffer);
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create a random client ID
    String clientId = "ESP_JebaoDP4-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str(), mqtt_user, mqtt_password)) {
      Serial.println("connected");
      
      // Once connected, publish an announcement...
      client.publish("dosing/DP4", "dosing/DP4 is Online");
      // ... and resubscribe
      client.subscribe("dosing/DP4/+");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.print(clientId);
      
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  
  setup_shift();
  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname("JebaoDP4");

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"333");

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
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Check if it's time to publish
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    
    publishShiftRegisterState();
  }
  
  // Check if it's time to update "alive" message
  if (currentMillis - lastAliveTimestamp >= 5000) { // Update every 5 seconds
    lastAliveTimestamp = currentMillis;
    publishAliveMessage();
  }
  
  if (!client.connected()) {
    reconnect();
  }
  
  client.loop();
  
  delay(10);
  ArduinoOTA.handle();
}
