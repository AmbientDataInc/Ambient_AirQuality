/*
 * SparkFunのCCS811ライブラリーを使い、30秒毎にCO2、TVOCの値を読むサンプルプログラム。
 */
#include <ESP8266WiFi.h>
#include <Wire.h>
#include "SparkFunCCS811.h"

#define PERIOD 30

#define CCS811_ADDR 0x5B // Default I2C Address

// ESP8266のピンの定義

#define SDA 13
#define SCL 14
#define CCS811_HW_RESET 5
#define CCS811_WAKE 4

CCS811 ccs811(CCS811_ADDR);

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

void readCCS811();
uint16_t CO2, TVOC;

void setup()
{
    Serial.begin(115200);
    delay(100);
    Serial.println("\r\nCCS811 read test.");

    pinMode(CCS811_WAKE, OUTPUT);
    ccs811_hw_reset();
    ccs811_wake();

    Wire.begin(SDA, SCL);

    CCS811Core::status returnCode = ccs811.begin(); // CCS811を初期化
    if (returnCode != CCS811Core::SENSOR_SUCCESS) { // 初期化に失敗したら
        Serial.print(".begin() returned with an error: ");
        Serial.println(returnCode, HEX);
        while (1) {
            delay(0); //　プログラムを停止する
        }
    }
    ccs811_sleep();
}

void readCCS811()
{
    ccs811_wake();
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

    readCCS811();
  
    Serial.print("CO2: ");
    Serial.print(CO2);
    Serial.print(", TVOC: ");
    Serial.println(TVOC);

    t = millis() - t;
    t = (t < PERIOD * 1000) ? (PERIOD * 1000 - t) : 1;
    delay(t);
}

