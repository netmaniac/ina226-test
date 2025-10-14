#include <Arduino.h>
#include "defines.h"
#include <ESP8266WiFi.h>

// jeśli wysyłka do influxa UDP
//#include <WiFiUdp.h>

//wersja HTTP API do influxa
#include <ESP8266HTTPClient.h>

#include <ArduinoOTA.h>
#include "Wire.h"
#include "INA226.h"
INA226 INA(0x40);

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
    delay(2000);
    Serial.begin(115200); // Uruchomienie portu szeregowego z prędkością 9600
    Serial.println("Boot....");
    Serial.print(F("Chip ID:"));
    Serial.println(String(ESP.getChipId(),10));
    connectWiFi();
    ArduinoOTA.begin();

    Wire.begin(D4, D3);
    if (!INA.begin() )
    {
        Serial.println("could not connect. Fix and Reboot");
    }
    int rc;
    // rc=INA.setMaxCurrentShunt(2, 0.1);
    // Serial.printf("rc=%d isCal=%d LSB(uA)=%.1f\n", rc, INA.isCalibrated(), INA.getCurrentLSB_uA());
    rc=INA.configure(0.1, 0.024719238);
    Serial.printf("rc=%d isCal=%d LSB(uA)=%.1f\n", rc, INA.isCalibrated(), INA.getCurrentLSB_uA());
    Serial.print("Wait for register.... ");
    delay(1000);
    Serial.println(String(INA.getRegister(0x5),16));



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
const unsigned long INTERVAL = 120*1000;
unsigned long last_send = 0;

void loop() {
    Serial.println("\nBUS\tSHUNT\tCURRENT\tPOWER");
    for (int i = 0; i < 20; i++)
    {
        Serial.print(INA.getBusVoltage(), 3);
        Serial.print("\t");
        Serial.print(INA.getShuntVoltage_mV(), 3);
        Serial.print("\t");
        Serial.print(INA.getCurrent_mA(), 3);
        Serial.print("\t");
        Serial.print(INA.getPower_mW(), 3);
        Serial.println();
        delay(1000);
    }


    if (millis() - last_send >= INTERVAL) {
        String postData;
        postData = String(MEASUREMENT_NAME) + String(F(",host=esp8266-")) + String(ESP.getChipId()) + F(" ");
        postData += "uptime=" + String(millis());
        postData += ",reading=" + String(12.3);
        sendToDB(postData);
        last_send = millis();
    }
    ArduinoOTA.handle();

}
