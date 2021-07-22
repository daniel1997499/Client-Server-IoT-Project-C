#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
//used for strcat()
#include <string.h>
//Temperature sensor DHT11
//#include <DHT.h>
#include "DHT.h"
//Base64URL
#include <Arduino.h>
#include "b64StreamLib.h"
//HMAC-SHA256
#include <Crypto.h>
//OLED SCREEN STUFF
#include <Adafruit_SSD1306.h>

//used for WIFI connection
const char* ssid = "V17Z";
const char* password = "6DMapenq";

//used for HTTP POST
String deviceID; //needs to be stored for the whole runtime

const char* serverName = "http://192.168.100.15:8088/api/";
const char* authenticate = "authenticate";
const char* sensordata = "sensordata";

//used for delay between requests
unsigned long authRetryDelay = 10000; //10 000 = 10 sec
unsigned long lastTrigger = 0;
unsigned long triggerDelay = 10000; // 10 sec

//DHT stuff
DHT dht(D5, DHT11);

//PIR stuff
#define pirPin D6

//MQ4 stuff
#define mq4Pin A0

//used for Base64URL encoding/decoding
b64StreamLib toTest = b64StreamLib();
char deviceName[12] = "NodeMCU#001";

//used for HMAC-SHA256 encoding
#define KEY_LENGTH 34
String strHmac = "";

//CONSTANTS FOR OLED SCREEN
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
void initDisplay(void) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.cp437(true);
}
void displayPrint(String text) {
  display.print(text);
  display.display();
}
void displayPrintln(String text) {
  display.println(text);
  display.display();
}
String lastValueOfTemp = "";
String lastValueOfHumid = "";
String lastValueOfMq4 = "";
//need to always display temp/humidity and mq4 data 
void setup() {
  Serial.begin(9600);
  while (!Serial){}
  delay(100);
  pinMode(pirPin, INPUT);
  pinMode(mq4Pin, INPUT);
  dht.begin();
  delay(100);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
   Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  initDisplay();
//============================  
  //WIFI STUFF
  delay(1000);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.println("Connecting..."); //print to display
  displayPrintln("Connecting...");
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("."); //print to display
    displayPrint(".");
  }
  Serial.println();
  Serial.print("Connected to WiFi network with IP Address: "); //print to display
  initDisplay();
  displayPrintln("Connected to WiFi");
  displayPrint("network with IP Address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip); //print to display
  displayPrintln(ip.toString());
  delay(2000);
  //WIFI STUFF END
//============================
  //Base64URL ENCODING
  toTest.initBuffer(64);
  toTest.writeCharArray(deviceName);
  String tmpStr = String(toTest.getEncodedBuffer());
  char base64name[20]; //toTest.getActualLength()
  tmpStr.toCharArray(base64name, sizeof(base64name));

  char address[20];
  sprintf(address, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]); //sprintf prints to char array instead of std:out
  toTest.initBuffer(64);
  toTest.writeCharArray(address);
  tmpStr = String(toTest.getEncodedBuffer());
  char base64address[50]; //toTest.getActualLength()
  tmpStr.toCharArray(base64address, sizeof(base64address));
  
  //Base64URL DECODING
//  toTest.writeEncodedCharArray("Tm9kZU1DVTEyMyE");
//  Serial.println(toTest.getEncodedBuffer());
//  for(char c = toTest.readChar(); toTest.getStatus() == B64STREAMLIB_STATUS_READING; c = toTest.readChar()) {
//    Serial.print(c);
//  }
//  Serial.println();
//  Serial.println(toTest.getActualLength());
  //Base64URL ENCODING END
//============================
  //HMAC-SHA256 HASHING
  byte key[KEY_LENGTH] = {'s','g','f','d','2','3','#','@','%','%','!','f','3','r','g','t','4','3','a','f','a','h','r','t','h','j','s','d','a','j','f','g','j','0'};
  SHA256HMAC hmac(key, KEY_LENGTH);
  int sizeForArray = sizeof(base64name) + sizeof(base64address);
  char buf[sizeForArray];
  strcpy(buf, base64name);
  strcat(buf, base64address);
  hmac.doUpdate(buf, strlen(buf));
  byte authCode[SHA256HMAC_SIZE];
  hmac.doFinal(authCode);
  for (byte i=0; i < SHA256HMAC_SIZE; i++) {
      if (authCode[i]<0x10) {
        strHmac += '0'; 
      }
      strHmac += String(authCode[i], HEX);
  }
  delay(1000);
  //HMAC-SHA256 HASHING END
//============================
  //HTTP STUFF
  Serial.println("Authenticating..."); //print to display
  initDisplay();
  displayPrintln("Authenticating...");
  delay(1000);
  while (WiFi.status()== WL_CONNECTED) { //don't advance until id is received
    HTTPClient http;
    char combined[50] = {0};
    strcat(combined, serverName);
    strcat(combined, authenticate);
    http.begin(combined);
    http.addHeader("Authorization", strHmac);
    http.addHeader("Content-Type", "application/json");
    String postString = String("{\"name\":\"" + String(deviceName) + "\",\"address\":\"" + ip.toString() + "\"}");
    int httpResponseCode = http.POST(postString);
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    if (httpResponseCode == HTTP_CODE_OK) {
      Serial.println("OK");
      const String& payload = http.getString();
      int pointIndex = payload.lastIndexOf(".");
      deviceID = payload.substring(0, pointIndex);
      break;
    }
    else {
      Serial.println("Resending..."); //print to display
      initDisplay();
      displayPrintln("Resending...");
    }
    http.end();
    delay(authRetryDelay); //10 sec
  }
  Serial.println("Exiting setup"); //print to display
  initDisplay();
  displayPrintln("Exiting setup");
  delay(2000);
}

void loop() {
  if(WiFi.status()== WL_CONNECTED) {
    if (digitalRead(pirPin) == HIGH) {
      if (millis() - lastTrigger > triggerDelay) {
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        int mq4Value = analogRead(mq4Pin); //10bit - from 0 to 1023
        lastValueOfTemp = String(t);
        lastValueOfHumid = String(h);
        lastValueOfMq4 = String(mq4Value);
        Serial.print("MQ4 value: ");
        Serial.println(mq4Value);
        //mq4 value lowest(300ppm) 1.5V
        //mq4 value highest(10 000ppm) 4V
        if (isnan(t) || isnan(t)) {
          Serial.println("Failed to read from DHT sensor!");
        }
        else {
          if (h > 0 && h < 100 && t > 0 && t < 50) { //exclude incorrect values
            Serial.println("Posting sensor data"); //print to display
            initDisplay();
            displayPrintln("Posting sensor data");
            char deviceIDArray[deviceID.length()+1];
            deviceID.toCharArray(deviceIDArray, deviceID.length()+1);
            toTest.initBuffer(64);
            toTest.writeCharArray(deviceIDArray);
            String tmpStr = String(toTest.getEncodedBuffer());
            char base64id[5];
            tmpStr.toCharArray(base64id, sizeof(base64id));
  
            char sensorTypeArray[6] = "DHT11";
            toTest.initBuffer(64);
            toTest.writeCharArray(sensorTypeArray);
            tmpStr = String(toTest.getEncodedBuffer());
            char base64sensorType[10];
            tmpStr.toCharArray(base64sensorType, sizeof(base64sensorType));                    
  
            char dataTypeArray[12] = "temperature";
            toTest.initBuffer(64);
            toTest.writeCharArray(dataTypeArray);
            tmpStr = String(toTest.getEncodedBuffer());
            char base64dataType[20];
            tmpStr.toCharArray(base64dataType, sizeof(base64dataType));
  
            char dataArray[3];
            String stringH = String(round(t));
            stringH.toCharArray(dataArray, sizeof(dataArray));
            toTest.initBuffer(64);
            toTest.writeCharArray(dataArray);
            tmpStr = String(toTest.getEncodedBuffer());
            char base64data[5];
            tmpStr.toCharArray(base64data, sizeof(base64data));
            
            byte key[KEY_LENGTH] = {'s','g','f','d','2','3','#','@','%','%','!','f','3','r','g','t','4','3','a','f','a','h','r','t','h','j','s','d','a','j','f','g','j','0'};
            SHA256HMAC hmac(key, sizeof(key));
            int lengthForArray = sizeof(base64id) + sizeof(base64sensorType) + sizeof(base64dataType) + sizeof(base64data);
            char buf[lengthForArray];
            strcpy(buf, base64id);
            strcat(buf, base64sensorType);
            strcat(buf, base64dataType);
            strcat(buf, base64data);
            hmac.doUpdate(buf, strlen(buf));
            byte authCode[SHA256HMAC_SIZE];
            hmac.doFinal(authCode);
            String tmpHmac = "";
            for (byte i=0; i < SHA256HMAC_SIZE; i++) {
                if (authCode[i]<0x10) {
                  tmpHmac += '0'; 
                }
                tmpHmac += String(authCode[i], HEX);
            }
            delay(1000);          
            //base64 encode and hmac end

            char combined[50] = {0};
            strcat(combined, serverName);
            strcat(combined, sensordata);
            HTTPClient http;
            http.begin(combined);
            http.addHeader("Authorization", tmpHmac);
            http.addHeader("Content-Type", "application/json");
            String postString = String("{\"deviceId\":\"" + deviceID + "\",\"sensor\":\"" + String(sensorTypeArray) + "\",\"dataType\":\"" + String(dataTypeArray) + "\",\"data\":\"" + String(dataArray) + "\"}");
            int httpResponseCode = http.POST(postString);
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
  //          if (http... == 400 || 300 || 302 || -1) {}
            if (httpResponseCode == HTTP_CODE_OK) {
              Serial.println("OK");
              const String& payload = http.getString();
            }
            http.end();
            //
            //Now send humidity the same way
            //Then MQ4 data
            //
          }
        }
        lastTrigger = millis();
        initDisplay();
        displayPrintln("Standby");
//        lastValueOfTemp = String(t);
//        lastValueOfHumid = String(h);
//        lastValueOfMq4 = String(mq4Value);
        displayPrint("last temp: ");
        displayPrintln(lastValueOfTemp);
        displayPrint("last humid: ");
        displayPrintln(lastValueOfHumid);
        displayPrint("last CO2: ");
        displayPrintln(lastValueOfMq4);
      }
    }
  }
}
