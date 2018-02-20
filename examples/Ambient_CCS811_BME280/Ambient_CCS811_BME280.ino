/*
 * SparkFunのCCS811ライブラリーを使い、60秒毎にCO2、TVOCの値を読み、Ambientに送信する。
 * BME280で温度、湿度、気圧を測定し、温度、湿度を使ってCCS811のデーター補正をしている
 */
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "BME280.h"
#include "SparkFunCCS811.h"
#include "Ambient.h"

extern "C" {
#include "user_interface.h"
}

#define PERIOD 60

#define CCS811_ADDR 0x5B // Default I2C Address

// ESP8266のピンの定義

#define SDA 13
#define SCL 14
#define CCS811_HW_RESET 5
#define CCS811_WAKE 4

BME280 bme280;
CCS811 ccs811(CCS811_ADDR);

WiFiClient client;
Ambient ambient;

const char* ssid = "...ssid...";
const char* password = "...password...";

unsigned int channelId = 100; // AmbientのチャネルID
const char* writeKey = "...writeKey..."; // ライトキー

// CCS811のnRESETピンをLOWにしてリセットする
void ccs811_hw_reset() {
    pinMode(CCS811_HW_RESET, OUTPUT);
    digitalWrite(CCS811_HW_RESET, LOW);
    delay(10);
    digitalWrite(CCS811_HW_RESET, HIGH);
}

// CCS811のnWAKEピンをLOWにしてI2C通信できるようにする
void ccs811_wake() {
    digitalWrite(CCS811_WAKE, LOW);
    delay(10);
}

// CCS811のnWAKEピンをHIGHにしてI2C通信を止め、消費電力を下げる
void ccs811_sleep() {
    digitalWrite(CCS811_WAKE, HIGH);
}

void readCCS811(float humid, float temp);
uint16_t CO2, TVOC;

void setup()
{
    wifi_set_sleep_type(LIGHT_SLEEP_T);

    Serial.begin(115200);
    delay(100);
    Serial.println("\r\nAir Qualty Sensor.");

    WiFi.begin(ssid, password);  //  Wi-Fi APに接続
    while (WiFi.status() != WL_CONNECTED) {  //  Wi-Fi AP接続待ち
        delay(100);
    }

    Serial.print("WiFi connected\r\nIP address: ");
    Serial.println(WiFi.localIP());

    pinMode(CCS811_WAKE, OUTPUT);
    ccs811_hw_reset(); // CCS811のリセット
    ccs811_wake(); // CCS811をI2C通信可能にする

    Wire.begin(SDA, SCL); // I2C通信を初期化

    bme280.begin(); // BME280の初期化

    CCS811Core::status returnCode = ccs811.begin(); // CCS811を初期化
    if (returnCode != CCS811Core::SENSOR_SUCCESS) { // 初期化に失敗したら
        Serial.print(".begin() returned with an error: ");
        Serial.println(returnCode, HEX);
        while (1) {
            delay(0); //　プログラムを停止する
        }
    }

    ambient.begin(channelId, writeKey, &client); // チャネルIDとライトキーを指定してAmbientの初期化

    ccs811_sleep(); // CCS811のI2C通信を止める
}

void readCCS811(float humid, float temp)
{
    ccs811_wake();
    // BME280で読んだ温度、湿度の値を使って補正をおこなう
    ccs811.setEnvironmentalData(humid, temp);
    CO2 = 0;
    while (CO2 <= 400 || CO2 > 8192) { // CCS811 Datasheet よりCO2の値は400〜8192ppmとのことで、
                                       // それ以外の範囲の値を読み飛ばす
        long t = millis();
        
        while (!ccs811.dataAvailable()) {
            delay(100);
            long e = millis() - t;
            if ((e) > 3000) {
                // 3秒以上データーが読み出し可能にならなければCCS811をリセットして再起動する
                Serial.println(e);
                t = millis();
                Serial.println("data unavailable for too long.");
                ccs811_hw_reset();
                CCS811Core::status returnCode = ccs811.begin(); // CCS811を初期化
                if (returnCode != CCS811Core::SENSOR_SUCCESS) { // 初期化に失敗したら
                    Serial.print(".begin() returned with an error: ");
                    Serial.println(returnCode, HEX);
                    while (1) {
                        delay(0); //　プログラムを停止する
                    }
                }
            }
        }
        ccs811.readAlgorithmResults();
        CO2 = ccs811.getCO2();
        TVOC = ccs811.getTVOC();
    }
    ccs811_sleep();
}

void loop()
{
    int t = millis();
    float temp, humid, pressure;

    // BME280で温度、湿度、気圧を測定する
    temp = (float)bme280.readTemperature();
    humid = (float)bme280.readHumidity();
    pressure = (float)bme280.readPressure();

    // 温度、湿度を渡して、CO2、TVOCの値を測定する
    readCCS811(humid, temp);
  
    Serial.print("CO2: ");
    Serial.print(CO2);
    Serial.print(", TVOC: ");
    Serial.print(TVOC);

    Serial.print(", temp: ");
    Serial.print(temp);
    Serial.print(", humid: ");
    Serial.print(humid);
    Serial.print(", pressure: ");
    Serial.println(pressure);

    // 温度、湿度、気圧、CO2、TVOCの値をAmbientに送信する
    ambient.set(1, String(temp).c_str());
    ambient.set(2, String(humid).c_str());
    ambient.set(3, String(pressure).c_str());
    ambient.set(4, CO2);
    ambient.set(5, TVOC);
    ambient.send();

    t = millis() - t;
    t = (t < PERIOD * 1000) ? (PERIOD * 1000 - t) : 1;
    delay(t);
}

