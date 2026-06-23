#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <ArduinoJson.h>

// Bibliothèques Capteurs
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_INA219.h>

// ─── CONFIGURATION ────────────────────────────────────────────────────────────
const char* ssid          = "Infinix HOT 11"; 
const char* password      = "2026fatyyy"; 

// !!! ATTENTION : Thabbet fil IP mta3 PC mte3ek tawa b-ipconfig !!!
const char* serverIPStr   = " 192.168.152.96"; 
const char* securityKey   = "123456";

#define DEFAULT_SLEEP_SEC  60
#define WIFI_TIMEOUT_SEC   15

Adafruit_BME280 bme;
Adafruit_INA219 ina219;

void goToSleep(uint32_t seconds) {
  Serial.println("Mise en veille capteurs...");
  ina219.powerSave(true);
  Serial.printf("Deep Sleep : %u secondes\n", seconds);
  Serial.flush();
  esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== ESP32 IoT — Démarrage ===");

  // 1. Initialisation I2C et capteurs
  Wire.begin(21, 22);
  if (!bme.begin(0x76)) {
    Serial.println("[ERREUR] BME280 non détecté.");
    goToSleep(DEFAULT_SLEEP_SEC);
  }
  if (!ina219.begin()) {
    Serial.println("[ERREUR] INA219 non détecté.");
    goToSleep(DEFAULT_SLEEP_SEC);
  }

  // 2. Connexion WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connexion WiFi");

  uint8_t attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts >= WIFI_TIMEOUT_SEC * 2) {
      Serial.println("\n[ERREUR] WiFi impossible.");
      goToSleep(DEFAULT_SLEEP_SEC);
    }
  }
  Serial.printf("\n[OK] WiFi connecté — IP locale : %s\n", WiFi.localIP().toString().c_str());

  // 3. Skip mDNS (On utilise l'IP fixe directement)
  Serial.println("[OK] Utilisation de l'IP fixe : " + String(serverIPStr));

  // 4. Lecture des capteurs
  float temp  = bme.readTemperature();
  float hum   = bme.readHumidity();
  float press = bme.readPressure() / 100.0F;
  float volt  = ina219.getBusVoltage_V();
  float mA    = ina219.getCurrent_mA();

  Serial.printf("Temp=%.1f°C Hum=%.1f%% V=%.2fV\n", temp, hum, volt);

  // 5. Envoi HTTP POST
  HTTPClient http;
  String url = "http://192.168.193.96:5000/update";
  Serial.println("Tentative d'envoi vers : " + url);

  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["key"]   = securityKey;
  doc["temp"]  = round(temp * 10) / 10.0;
  doc["hum"]   = round(hum * 10) / 10.0;
  doc["press"] = round(press * 10) / 10.0;
  doc["volt"]  = round(volt * 100) / 100.0;
  doc["mA"]    = round(mA * 10) / 10.0;

  String jsonPayload;
  serializeJson(doc, jsonPayload);

  int httpCode = http.POST(jsonPayload);
  
  if (httpCode == 200) {
    Serial.println("[OK] Data envoyée !");
    Serial.println("Réponse : " + http.getString());
  } else {
    Serial.printf("[ERREUR] HTTP %d\n", httpCode);
  }

  http.end();
  WiFi.disconnect(true);
  
  // 8. Deep Sleep
  goToSleep(DEFAULT_SLEEP_SEC);
}

void loop() {}