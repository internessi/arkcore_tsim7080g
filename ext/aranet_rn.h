/**
 * aranetRn.h
 * BLE-Schnittstelle zum Auslesen von Aranet-Radon-Sensoren
 * 
 * Diese Datei enthält die Logik zum Scannen nach Aranet-Sensoren per BLE, 
 * zum Aufbau einer Verbindung und zum Auslesen der Sensordaten wie Temperatur, 
 * Luftdruck, Luftfeuchtigkeit und Radonkonzentration.
 * 
 * Enthaltene Funktionen und Klassen:
 * - class ScanCallbacks: BLE-Callback zum Erkennen von Aranet-Geräten im Scan
 * - scanAranetRn(): Startet BLE-Scan und erkennt Aranet-Sensoren
 * - readAranetRn(): Verbindet sich mit erkanntem Sensor und liest Messwerte
 * 
 * Globale Variablen speichern die Messwerte und weitere Metainformationen:
 * float temperature;                   // Messwert im Hauptprogramm
 * float pressure;                      // Messwert im Hauptprogramm
 * float humidity;                      // Messwert im Hauptprogramm
 * uint16_t radon;                      // Messwert im Hauptprogramm
 * const char* statusScanAranetRn = ""; // Statusvariable für Scan im Hauptprogramm
 */
uint16_t sensorType;
uint16_t interval;
uint16_t age;

/**
 * ScanCallbacks
 * 
 * Diese Callback-Klasse wird beim BLE-Scan verwendet. Sie überprüft jedes
 * gefundene Gerät und prüft, ob der Gerätename mit "Aranet" beginnt.
 * Wenn ein passendes Gerät gefunden wird:
 *   - wird der Scan gestoppt
 *   - das Gerät in advDevice gespeichert
 *   - das Flag doConnect gesetzt, um später die Verbindung aufzubauen
 */
class ScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        std::string name = advertisedDevice->getName();
        if (!name.empty() && name.substr(0, 6) == "Aranet") {
            NimBLEDevice::getScan()->stop();
            advDevice = advertisedDevice;
            doConnect = true;
        }
    }
} scanCallbacks;

/**
 * scanAranetRn
 * 
 * Führt einen BLE-Scan durch und sucht nach einem Aranet-Sensor.
 * Wenn ein passendes Gerät gefunden wird, wird es global gespeichert.
 * 
 * Rückgabe:
 *   - "ok"          → Aranet-Gerät gefunden
 *   - "not_found"   → kein Gerät im Scanzeitraum gefunden
 *   - "scan_failed" → Scan konnte nicht gestartet werden
 */
const char* scanAranetRn() {
    advDevice = nullptr;
    doConnect = false;

    NimBLEDevice::init("NimBLE-Client");
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_SC);
    NimBLEDevice::setPower(3);
    NimBLEScan* pScan = NimBLEDevice::getScan();

    if (!pScan) {
        Serial.println("scan_failed");
        return "scan_failed";
    }

    pScan->setScanCallbacks(&scanCallbacks, false);
    pScan->setInterval(100);
    pScan->setWindow(100);
    pScan->setActiveScan(true);
    pScan->start(scanTimeMs, false);

    uint32_t startTime = millis();
    while (advDevice == nullptr && millis() - startTime < scanTimeMs + 1000) {
        delay(100);
    }

    if (advDevice != nullptr) {
        Serial.print("Gefunden: ");
        Serial.print(advDevice->getName().c_str());
        Serial.print(" @ ");
        Serial.println(advDevice->getAddress().toString().c_str());
        return "ok";
    } else {
        Serial.println("Kein Aranet-Sensor gefunden.");
        return "not_found";
    }
}

void cleanupAfterClient(NimBLEClient* client) {
    NimBLEDevice::deleteClient(client);
    NimBLEDevice::getScan()->clearResults();
    advDevice = nullptr;
}

/**
 * readAranetRn
 * 
 * Verbindet sich mit dem zuletzt gefundenen Aranet-Gerät (BLE) und liest die Sensorwerte.
 * Die Werte werden in globale Variablen geschrieben.
 * 
 * Rückgabe:
 *   - "ok"                → Sensorwerte erfolgreich gelesen
 *   - "no_adv"            → kein Gerät verfügbar (advDevice == nullptr)
 *   - "reconnect_failed"  → Reconnect zu bekanntem Gerät fehlgeschlagen
 *   - "no_client"         → kein freier Client verfügbar
 *   - "connect_failed"    → Verbindung zum Gerät schlug fehl
 *   - "service_not_found" → erwarteter Service wurde nicht gefunden
 *   - "char_not_found"    → Characteristic nicht vorhanden oder nicht lesbar
 *   - "read_invalid"      → gelesene Daten ungültig (zu kurz)
 */
const char* readAranetRn() {
    if (advDevice == nullptr) {
        Serial.print("no_adv");
        return "no_adv";
    }

    NimBLEClient* pClient = NimBLEDevice::createClient();
    pClient->setConnectionParams(12, 12, 0, 150);
    pClient->setConnectTimeout(5000);
    //Serial.print("advDevice connectable: ");
    //Serial.println(advDevice->isConnectable() ? "YES" : "NO");
    //Serial.print("Connect attempt to: ");
    //Serial.println(advDevice->getAddress().toString().c_str());
    
    if (!pClient->connect(advDevice)) {
        cleanupAfterClient(pClient);
        Serial.print("connect_failed");
        loadSensorDataFromNVS();
        delay(1000);
        return "connect_failed";
    }

    NimBLERemoteService* pSvc = pClient->getService("FCE0");
    if (!pSvc) {
        pClient->disconnect();
        cleanupAfterClient(pClient);
        Serial.print("service_not_found");
        return "service_not_found";
    }

    NimBLERemoteCharacteristic* pChr = pSvc->getCharacteristic("f0cd3003-95da-4f4b-9ac8-aa55d312af0c");
    if (!pChr || !pChr->canRead()) {
        pClient->disconnect();
        cleanupAfterClient(pClient);
        Serial.print("char_not_found");
        return "char_not_found";
    }

    std::string value = pChr->readValue();
    if (value.length() < 15) {
        pClient->disconnect();
        cleanupAfterClient(pClient);
        Serial.print("read_invalid");
        return "read_invalid";
    }

    sensorType = (uint8_t)value[0] | ((uint8_t)value[1] << 8);
    interval   = (uint8_t)value[2] | ((uint8_t)value[3] << 8);
    age        = (uint8_t)value[4] | ((uint8_t)value[5] << 8);
    uint16_t tempRaw  = (uint8_t)value[7] | ((uint8_t)value[8] << 8);
    uint16_t pressRaw = (uint8_t)value[9] | ((uint8_t)value[10] << 8);
    uint16_t humRaw   = (uint8_t)value[11] | ((uint8_t)value[12] << 8);
    radon     = (uint8_t)value[13] | ((uint8_t)value[14] << 8);

    temperature = tempRaw / 20.0;
    pressure    = pressRaw / 10.0;
    humidity    = humRaw / 10.0;

    pClient->disconnect();
    cleanupAfterClient(pClient);
    Serial.print("ok");
    return "ok";
}

void scanSensor() {
    statusScanAranetRn = scanAranetRn();
    Serial.printf("Scan: %s - %s\n", statusScanAranetRn, advDevice ? advDevice->getName().c_str() : "kein Gerät");
}

void readSensor() {
    const char* statusRead = readAranetRn();
    Serial.printf("Read: %s - %u Bq/m³ - %.1f °C - %.1f %%RH\n", statusRead, radon, temperature, humidity);
}
