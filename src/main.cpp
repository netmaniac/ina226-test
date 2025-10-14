#include <Arduino.h>
#include "defines.h"
#include "float.h"
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
    http.addHeader("Content-Type", "text/plain"); //Specify content-type header
    int httpCode = http.POST(postData); //Send the request
    String payload = http.getString(); //Get the response payload

    Serial.println(httpCode); //Print HTTP return code
    Serial.println(payload); //Print request response payload

    http.end(); //Close connection

    //wysyłka UDP
    //Influx Nettigo (ifx.nettigo.pl)
    //    IPAddress influx(172, 104, 158, 225);
    //    udp.beginPacket(influx, 8086);
    //    udp.print(postData);
    //    udp.endPacket();
}

//*****************************
const unsigned long INTERVAL = 60 * 1000;
unsigned long last_send = 0;
unsigned long lastMeasure = 0;
unsigned long lastAggregation = 0;

const byte MAX_SAMPLES_IN_SECOND = 200;
float DATA_AGGREGATE[60][3];
float DATA_SECOND[MAX_SAMPLES_IN_SECOND][3];

float MAX[3];
float MIN[3];

enum DataIdx { VBUS_IDX, CURR_IDX, PWR_IDX };

unsigned sampleCount = 0;
unsigned long aggregateCount = 0;




//*****************************
void setup() {
    delay(2000);
    Serial.begin(115200); // Uruchomienie portu szeregowego z prędkością 9600
    Serial.println("Boot....");
    Serial.print(F("Chip ID:"));
    Serial.println(String(ESP.getChipId(), 10));
    connectWiFi();
    ArduinoOTA.begin();

    Wire.setClock(400000);
    Wire.begin(D4, D3);
    if (!INA.begin()) {
        Serial.println("could not connect. Fix and Reboot");
    }
    int rc;
    // rc=INA.setMaxCurrentShunt(2, 0.1);
    // Serial.printf("rc=%d isCal=%d LSB(uA)=%.1f\n", rc, INA.isCalibrated(), INA.getCurrentLSB_uA());
    rc = INA.configure(0.1, 0.024719238);
    Serial.printf("rc=%d isCal=%d LSB(uA)=%.1f\n", rc, INA.isCalibrated(), INA.getCurrentLSB_uA());
    Serial.print("Wait for register.... ");
    delay(1000);
    Serial.println(String(INA.getRegister(0x5), 16));
    INA.setAverage(INA226_4_SAMPLES);
    INA.setShuntVoltageConversionTime(INA226_588_us);
    INA.setBusVoltageConversionTime(INA226_588_us);
    lastAggregation = millis();
    lastMeasure = micros();
}



void resetAggregate() {
    for (int i = 0; i < 3; i++) {
        MAX[i] = 0;
        MIN[i] = FLT_MAX;
    }
    for (int i = 0; i < 60; i++) {
        for (int j = 0; j < 3; j++)
            DATA_AGGREGATE[i][j] = 0;
    }
    aggregateCount = 0;
}

void resetSecondStats() {
    for (int i = 0; i < MAX_SAMPLES_IN_SECOND; i++) {
        DATA_SECOND[i][0] = 0;
        DATA_SECOND[i][1] = 0;
        DATA_SECOND[i][2] = 0;
    }
    sampleCount = 0;
}

void getSample() {
    if (sampleCount == MAX_SAMPLES_IN_SECOND) { return; }

    DATA_SECOND[sampleCount][VBUS_IDX] = INA.getBusVoltage();
    DATA_SECOND[sampleCount][CURR_IDX] = INA.getCurrent_mA();
    DATA_SECOND[sampleCount][PWR_IDX] = INA.getPower_mW();

    if (DATA_SECOND[sampleCount][VBUS_IDX] < MIN[VBUS_IDX]) MIN[VBUS_IDX] = DATA_SECOND[sampleCount][VBUS_IDX];
    if (DATA_SECOND[sampleCount][VBUS_IDX] > MAX[VBUS_IDX]) MAX[VBUS_IDX] = DATA_SECOND[sampleCount][VBUS_IDX];

    if (DATA_SECOND[sampleCount][CURR_IDX] < MIN[CURR_IDX]) MIN[CURR_IDX] = DATA_SECOND[sampleCount][CURR_IDX];
    if (DATA_SECOND[sampleCount][CURR_IDX] > MAX[CURR_IDX]) MAX[CURR_IDX] = DATA_SECOND[sampleCount][CURR_IDX];

    if (DATA_SECOND[sampleCount][PWR_IDX] < MIN[PWR_IDX]) MIN[PWR_IDX] = DATA_SECOND[sampleCount][PWR_IDX];
    if (DATA_SECOND[sampleCount][PWR_IDX] > MAX[PWR_IDX]) MAX[PWR_IDX] = DATA_SECOND[sampleCount][PWR_IDX];
    sampleCount++;
}

void aggregate() {
    float sum[3] = {0, 0, 0};

    if (
        sampleCount == 0 || //nie ma danych
        aggregateCount == 60 //zebralismy już 60 próbek
        ) { return; }

    for (int i = 0; i < MAX_SAMPLES_IN_SECOND; i++) {
        for (int j = 0; j < 3; j++) {
            sum[j] += DATA_SECOND[i][j];
        }
    }
    for (int i = 0; i < 3; i++) {
        DATA_AGGREGATE[aggregateCount][i] = sum[i] / sampleCount;
    }
    aggregateCount++;
    resetSecondStats();

    lastAggregation = millis();
    lastMeasure = micros();
}

void loop() {
    if (micros() - lastMeasure >= 4700) {
        getSample();
        lastMeasure = micros();
    }
    if (millis() - lastAggregation > 1000) {
        Serial.printf("Samples taken: %d, aggregatCnt %d\n", sampleCount,aggregateCount);
        aggregate();
        lastAggregation = millis();
    }

    if (millis() - last_send >= INTERVAL && aggregateCount > 0 || aggregateCount == 60) {
        float sum[3] = {0, 0, 0};

        for (int i=0;i<aggregateCount;i++) {
            for (int j=0;j<3;j++) {
                sum[j] += DATA_AGGREGATE[i][j];
            }
        }
        String postData;
        postData = String(MEASUREMENT_NAME) + String(F(",host=esp8266-")) + String(ESP.getChipId()) + F(" ");
        postData += "uptime=" + String(millis());
        postData += ",avgCurr=" + String(sum[CURR_IDX]/aggregateCount, 2);
        postData += ",avgPwr=" + String(sum[PWR_IDX]/aggregateCount, 2);
        postData += ",avgV=" + String(sum[VBUS_IDX]/aggregateCount, 2);

        postData += ",maxV=" + String(MAX[VBUS_IDX], 2);
        postData += ",minV=" + String(MIN[VBUS_IDX], 2);

        postData += ",maxCurr=" + String(MAX[CURR_IDX], 2);
        postData += ",minCurr=" + String(MIN[CURR_IDX], 2);
        sendToDB(postData);
        resetSecondStats();
        resetAggregate();
        last_send = millis();
    }
    ArduinoOTA.handle();
}
