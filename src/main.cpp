#include <Arduino.h>
#include "defines.h"
#include <ESP8266WiFi.h>

// jeśli wysyłka do influxa UDP
//#include <WiFiUdp.h>

//wersja HTTP API do influxa
#include <ESP8266HTTPClient.h>

#include <ArduinoOTA.h>

extern const char *ssid;
extern const char *password;

#include "wifi.h"


//*****************************
#define CNT_LIMIT 20

void connectWiFi() {
    byte cnt = 0;
    WiFi.mode(WIFI_STA);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED && cnt++ < CNT_LIMIT) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected");
        Serial.println("IP address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("Failed connecting to network.");
    }

}

//*****************************
void setup() {
    Serial.begin(115200); // Uruchomienie portu szeregowego z prędkością 9600
    Serial.println("Boot....");
    Serial.print(F("Chip ID:"));
    Serial.println(String(ESP.getChipId(),10));
    connectWiFi();
    ArduinoOTA.begin();


}

//*****************************
void sendToDB(String postData) {
//    WiFiUDP udp;
    HTTPClient http;
    WiFiClient *client = new WiFiClient;

if (WiFi.status() != WL_CONNECTED) {
        connectWiFi();
    }
    Serial.println(postData);
    client->setTimeout(20000);

    //wspólna baza na jednorazowe testy
    // http.begin("http://ifx.nettigo.pl:8086/write?db=playground");      //Specify request destination
    http.begin(*client, "http://ifx.nettigo.pl:8086/write?db=playground");
    http.addHeader("Content-Type", "text/plain");  //Specify content-type header
    int httpCode = http.POST(postData);   //Send the request
    String payload = http.getString();                  //Get the response payload

    Serial.println(httpCode);   //Print HTTP return code
    Serial.println(payload);    //Print request response payload

    http.end();  //Close connection

    //wysyłka UDP
//Influx Nettigo (ifx.nettigo.pl)
//    IPAddress influx(172, 104, 158, 225);
//    udp.beginPacket(influx, 8086);
//    udp.print(postData);
//    udp.endPacket();

}

//*****************************
const unsigned long INTERVAL = 90*1000;
unsigned long last_send = 0;

void loop() {
    if (millis() - last_send >= INTERVAL) {
        String postData;
        postData = String(MEASUREMENT_NAME) + String(F(",host=esp8266-")) + String(ESP.getChipId()) + F(" ");
        postData += "uptime=" + String(millis());
        postData += ",reading=" + String(12.3);
        sendToDB(postData);
    }
    ArduinoOTA.handle();

}
