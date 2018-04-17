#include <ESP8266WiFi.h>
#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <Wire.h>
#include "bme280_i2c.h"
#include "SparkFunCCS811.h"
#include "Ambient.h"

#define PERIOD 60

static const int RXPin = 2, TXPin = 12;
SoftwareSerial ss(RXPin, TXPin, false, 256);
TinyGPSPlus gps;

#define SDA 13
#define SCL 14

#define CCS811_ADDR 0x5B // Default I2C Address
#define CCS811_HW_RESET 5
#define CCS811_WAKE 4

BME280 bme280(BME280_I2C_ADDR_PRIM);
CCS811 ccs811(CCS811_ADDR);

WiFiClient client;
Ambient ambient;

const char* ssid = "ssid";
const char* password = "password";

unsigned int channelId = 100; // AmbientのチャネルID
const char* writeKey = "writeKey"; // ライトキー

// CCS811のnRESETピンをLOWにしてリセットする
void ccs811_hw_reset() {
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
    Serial.begin(115200);
    delay(100);
    Serial.println("\r\nAir quality with location");
    ss.begin(9600);

    WiFi.begin(ssid, password);  //  Wi-Fi APに接続
    while (WiFi.status() != WL_CONNECTED) {  //  Wi-Fi AP接続待ち
        Serial.print(".");
        delay(1000);
    }

    Serial.print("WiFi connected\r\nIP address: ");
    Serial.println(WiFi.localIP());

    pinMode(CCS811_HW_RESET, OUTPUT);
    pinMode(CCS811_WAKE, OUTPUT);
    ccs811_hw_reset(); // CCS811のリセット
    ccs811_wake(); // CCS811をI2C通信可能にする

    Wire.begin(SDA, SCL);
    bme280.begin(); // BME280の初期化

    CCS811Core::status returnCode = ccs811.begin(); // CCS811を初期化
    if (returnCode != CCS811Core::SENSOR_SUCCESS) { // 初期化に失敗したら
        Serial.print(".begin() returned with an error: ");
        Serial.println(returnCode, HEX);
        while (1) {
            delay(0); //　プログラムを停止する
        }
    }
    ccs811_sleep(); // CCS811のI2C通信を止める

    ambient.begin(channelId, writeKey, &client); // チャネルIDとライトキーを指定してAmbientの初期化
}

void readCCS811(float humid, float temp)
{
    ccs811_wake(); // CCS811をI2C通信可能にする。
    // BME280で読んだ温度、湿度の値を使って補正をおこなう
    ccs811.setEnvironmentalData(humid, temp);
    CO2 = 0;
    while (CO2 <= 400 || CO2 > 8192) { // CCS811 Datasheet よりCO2の値は400〜8192ppmとのことで、
                                       // それ以外の範囲の値を読み飛ばす
        long t = millis();
        
        while (!ccs811.dataAvailable()) { // CCS811のデータが読み出し可能かチェックする。
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
        ccs811.readAlgorithmResults(); // CO2とTVOCの値をCSS811から読み出す。
        CO2 = ccs811.getCO2(); // CO2の値を取得する。
        TVOC = ccs811.getTVOC(); // TVOCの値を取得する。
    }
    ccs811_sleep(); // CCS811のI2C通信を停止する。
}

time_t lasttime = 0;

void loop()
{
    while (ss.available() > 0) {
        if (gps.encode(ss.read())) {
            break;
        }
        delay(0);
    }
    if ((millis() - lasttime) > PERIOD * 1000 && gps.location.isValid()) {
        char buf[16];

        Serial.print("\r\nElapse: "); Serial.println((millis() - lasttime) / 1000); 
        lasttime = millis();

        dtostrf(gps.location.lat(), 10, 6, buf);
        Serial.print("lat: "); Serial.print(buf);
        dtostrf(gps.location.lng(), 10, 6, buf);
        Serial.print(", lng: "); Serial.print(buf);
        dtostrf(gps.altitude.meters(), 4, 2, buf);
        Serial.print(", altitude: "); Serial.print(buf);

        struct bme280_data data;
        bme280.get_sensor_data(&data);
        dtostrf(data.temperature, 5, 2, buf);
        Serial.print(", temp: "); Serial.print(buf);
        dtostrf(data.humidity, 5, 2, buf);
        Serial.print(", humid: "); Serial.print(buf);
        dtostrf(data.pressure / 100, 6, 2, buf);
        Serial.print(", press: "); Serial.println(buf);

        // 温度、湿度を渡して、CO2、TVOCの値を測定する
        readCCS811(data.humidity, data.temperature);

        Serial.print("CO2: "); Serial.println(CO2);

        // 温度、湿度、気圧、CO2の値をAmbientに送信する
        ambient.set(1, data.temperature);
        ambient.set(2, data.humidity);
        ambient.set(3, data.pressure / 100);
        ambient.set(4, CO2);
        dtostrf(gps.location.lat(), 12, 8, buf);
        ambient.set(9, buf);
        dtostrf(gps.location.lng(), 12, 8, buf);
        ambient.set(10, buf);
        ambient.send();
    }
    delay(1000);
}

