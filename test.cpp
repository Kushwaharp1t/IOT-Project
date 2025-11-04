#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include "DHT.h"
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_BMP280.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// === DHT Sensor ===
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// === SD Card ===
#define SD_CS 5
bool sd_ok = false;

// === BH1750 Light Sensor ===
BH1750 lightMeter;

// === BMP280 Pressure Sensor ===
Adafruit_BMP280 bmp;

// === Water Level Sensor ===
#define WATER_PIN 34

// === Rain Sensor ===
#define RAIN_PIN 35

// === Soil Temperature Sensor (DS18B20) ===
OneWire oneWire(32);
DallasTemperature sensors(&oneWire);

// === Soil Moisture ===
int sensorPin = 33;
int sensorValue = 0;
int moisturePercent = 0;
int dryValue = 4095;
int wetValue = 2550;

// === Motor Relay ===
#define MOTOR_PIN 25
bool motorState = false;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("\n=== ESP32 Smart Farm System Booting ===");

  dht.begin();
  Wire.begin(21, 22);

  // Init SD Card
  if (!SD.begin(SD_CS)) {
    Serial.println("⚠ SD Card Mount Failed! Data will not be saved to SD.");
    sd_ok = false;
  } else {
    sd_ok = true;
    Serial.println(" SD Card initialized.");
    if (!SD.exists("/data.csv")) {
      File file = SD.open("/data.csv", FILE_WRITE);
      if (file) {
        file.println("Temperature(C),Humidity(%),Light(Lux),Pressure(hPa),WaterLevel(ADC),RainValue(ADC),SoilTemp(C),SoilMoisture(%),MotorState");
        file.close();
        Serial.println("CSV header written.");
      }
    }
  }

  // Init BH1750
  if (!lightMeter.begin()) {
    Serial.println("⚠ BH1750 init failed!");
  } else {
    Serial.println(" BH1750 ready.");
  }

  // Init BMP280
  if (!bmp.begin(0x76)) {
    Serial.println(" BMP280 not found!");
  } else {
    Serial.println(" BMP280 ready.");
  }

  sensors.begin();

  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
}

void loop() {
  // Read sensors
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  float lux = lightMeter.readLightLevel();
  float pressure = bmp.readPressure() / 100.0F;
  int waterLevel = analogRead(WATER_PIN);
  int rainValue = analogRead(RAIN_PIN);

  sensors.requestTemperatures();
  float soilTemp = sensors.getTempCByIndex(0);

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(3000);
    return;
  }

  sensorValue = analogRead(sensorPin);
  moisturePercent = map(sensorValue, dryValue, wetValue, 0, 100);
  moisturePercent = constrain(moisturePercent, 0, 100);

  // Motor logic
  if (moisturePercent < 35) {
    digitalWrite(MOTOR_PIN, HIGH);
    motorState = true;
  } else if (moisturePercent > 65) {
    digitalWrite(MOTOR_PIN, LOW);
    motorState = false;
  }

  // === Print to Serial Monitor with labels ===
  Serial.println("\n Sensor Data:");
  Serial.print("Temperature: "); Serial.print(temperature, 1); Serial.println(" °C");
  Serial.print("Humidity: "); Serial.print(humidity, 1); Serial.println(" %");
  Serial.print("Light: "); Serial.print(lux, 2); Serial.println(" lux");
  Serial.print("Pressure: "); Serial.print(pressure, 2); Serial.println(" hPa");
  Serial.print("Water Level: "); Serial.println(waterLevel);
  Serial.print("Rain Sensor: "); Serial.println(rainValue);
  Serial.print("Soil Temp: "); Serial.print(soilTemp, 1); Serial.println(" °C");
  Serial.print("Soil Moisture: "); Serial.print(moisturePercent); Serial.println(" %");
  Serial.print("Motor State: "); Serial.println(motorState ? "ON" : "OFF");
  Serial.println("-------------------");

  // === Build CSV row with all values ===
  String dataString = String(temperature, 1) + "," +
                      String(humidity, 1) + "," +
                      String(lux, 2) + "," +
                      String(pressure, 2) + "," +
                      String(waterLevel) + "," +
                      String(rainValue) + "," +
                      String(soilTemp, 1) + "," +
                      String(moisturePercent) + "," +
                      (motorState ? "ON" : "OFF");

  // Save to SD card
  if (sd_ok) logToSD(dataString);

  delay(60000); // log every 1 min
}
void logToSD(String data) {
  File file = SD.open("/data.csv", FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open file for writing!");
    return;
  }
  file.println(data);
  file.close();
  Serial.println("Logged:"+data);
}