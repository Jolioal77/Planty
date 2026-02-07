#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <ArduinoJson.h>

/* =====================
   CONFIG
===================== */
#define DHTPIN 4
#define DHTTYPE DHT11
#define SOIL_PIN 35
#define LDR_PIN 34

#define SERVICE_UUID        "fd418ddc-13e2-4fd1-8c29-dc51a06994c6"
#define CHARACTERISTIC_UUID "ad5e200a-dc37-4724-9e16-fd0416c17924"

DHT dht(DHTPIN, DHTTYPE);
BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

/* =====================
   BLE CALLBACKS
===================== */
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    // Use the global advertising object to restart advertising correctly
    BLEDevice::getAdvertising()->start();
  }
};

/* =====================
   SETUP
===================== */
void setup() {
  Serial.begin(115200);
  dht.begin(); // Initialize DHT sensor

  BLEDevice::init("ESP32-Planty");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Add PROPERTY_READ for Web Bluetooth compatibility along with NOTIFY
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ
  );

  pCharacteristic->addDescriptor(new BLE2902());

  pService->start();

  // Configure and start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID); // Make the service discoverable
  pAdvertising->setScanResponse(true);
  // Helps with iOS/macOS connection issues
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();

  Serial.println("Bluetooth device is now advertising");
}

/* =====================
   LOOP
===================== */
void loop() {
  // Read DHT11 sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();

  // Check if reads failed and handle gracefully
  if (isnan(temp) || isnan(hum)) {
    Serial.println("Failed to read from DHT sensor!");
    delay(2000);
    return;
  }

  // Read Soil moisture sensor
  int soilRaw = analogRead(SOIL_PIN);
  int soilPercent = map(soilRaw, 4095, 1500, 0, 100); // calibrate values
  soilPercent = constrain(soilPercent, 0, 100);

  // Read LDR sensor
  int ldrRaw = analogRead(LDR_PIN);
  int lux = map(ldrRaw, 0, 4095, 0, 2000); // estimate lux

  // Debug print to Serial (human readable)
  Serial.printf("Temp: %.1f C, Hum: %.1f %%, Light: %d lx, Soil: %d %%\n", temp, hum, lux, soilPercent);

  // Create JSON payload string with measurement units included
  // Example: {"temperature":"24.1 °C","light":"550 lx","soilMoisture":"40 %","humidity":"45.0 %"}
  DynamicJsonDocument doc(256);
  doc["temperature"] = String(temp, 1) + " °C";
  doc["light"] = String(lux) + " lx";
  doc["soilMoisture"] = String(soilPercent) + " %";
  doc["humidity"] = String(hum, 1) + " %";
  String payload;
  serializeJson(doc, payload);

  // Always print JSON payload to Serial so a serial client (browser Web Serial) can read the same data
  Serial.println(payload);

  // Update BLE characteristic value; notify only when connected
  if (pCharacteristic) {
    pCharacteristic->setValue(payload.c_str());
    if (deviceConnected) {
      pCharacteristic->notify();
    }
  }

  delay(3000); // send every 3 seconds
}
