/**
 * arkcore_tsim7070g
 * Allgemeine Unterprogramme
 */

uint32_t getCurrentUnixTime() {
  return unixTime + (millis() / 1000 - syncMillis);
}

String generateSerialFromChipId() {
  const char* baseChars = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // 32 eindeutige Zeichen
  const char* prefixChars = "ABCDEFGHJKLMNPQRSTUVWXYZ";       // 24 Buchstaben ohne I/O

  uint64_t mac = ESP.getEfuseMac();

  // FNV-1a Hash über 6 Byte MAC → 32 Bit Hash
  uint32_t hash = 2166136261UL;
  for (int i = 0; i < 6; i++) {
    hash ^= (mac >> (8 * i)) & 0xFF;
    hash *= 16777619UL;
  }

  // Erstes Zeichen (Buchstabe)
  char prefix = prefixChars[hash % 24];
  hash /= 24;

  // 4 Base32-Zeichen
  String result = "";
  for (int i = 0; i < 4; i++) {
    result = baseChars[hash % 32] + result;
    hash /= 32;
  }

  return String(prefix) + result;  // z. B. R9X7D
}

bool timeToReadSensor() {
    if (millis() - lastReadTime >= readInterval) {
        lastReadTime = millis();
        return true;
    }
    return false;
}

// Batteriemessung: nur GPIO35 intern (Faktor 2.0)
void measureBattery() {
  int raw = analogRead(35);
  float voltage = (raw / 4095.0) * 3.3 * 2.0;
  batteryVolts100 = voltage * 100 + 20; // Kalibrierwert anpassen

  if (batteryVolts100 == 20) {
    batteryVolts100 = 500;
  }

  Serial.print("BAT intern (35): ");
  Serial.print(batteryVolts100);
  Serial.println(" (x100 V)");
}

void loadCountersFromNVS() {
  Preferences prefs;
  prefs.begin("config", true);
  nvsSensorCounter = prefs.getUChar("sensorCount", 0);
  nvsGsmCounter    = prefs.getUChar("gsmCount", 0);
  prefs.end();
}

void saveCountersToNVS() {
  Preferences prefs;
  prefs.begin("config", false);
  prefs.putUChar("sensorCount", nvsSensorCounter);
  prefs.putUChar("gsmCount", nvsGsmCounter);
  prefs.end();
}

void saveSensorDataToNVS() {
  Serial.println("SAVE: " + String(temperature) + " - " + String(pressure) + " - " + String(radon) + " - " + String(humidity));
  Preferences prefs;
  prefs.begin("sensor", false);
  prefs.putFloat("temp", temperature);
  prefs.putFloat("press", pressure);
  prefs.putFloat("hum", humidity);
  prefs.putUShort("radon", radon);
  prefs.end();
}

void loadSensorDataFromNVS() {
  Preferences prefs;
  prefs.begin("sensor", true);
  temperature = prefs.getFloat("temp", 0.0);
  pressure    = prefs.getFloat("press", 0.0);
  humidity    = prefs.getFloat("hum", 0.0);
  radon       = prefs.getUShort("radon", 0);
  prefs.end();
  Serial.println("LOAD: " + String(temperature) + " - " + String(pressure) + " - " + String(radon) + " - " + String(humidity));
}

void blinkBlueLed3x() {
  pinMode(12, OUTPUT);  // LED kontrollieren

  for (int i = 0; i < 3; i++) {
    digitalWrite(12, LOW);   // LED AN (invertiert)
    delay(200);
    digitalWrite(12, HIGH);  // LED AUS
    delay(200);
  }

  pinMode(12, INPUT);  // wieder in stromsparenden Zustand
}

