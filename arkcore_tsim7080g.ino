#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// BLE-Variablen
static const NimBLEAdvertisedDevice* advDevice;
static bool doConnect = false;
static const uint32_t scanTimeMs = 5000;
static uint32_t lastReadTime = 0;
static const uint32_t readInterval = 10 * 60 * 1000;

String SN;
float temperature = 20;
float pressure = 900;
float humidity = 40;
uint16_t radon = 10;
const char* statusScanAranetRn = "";
char printLine[20];

uint16_t batteryVolts100 = 0; // interne Teilung, in 0.01 V (z. B. 331 = 3.31 V)
const uint64_t sleepMinute  = 60ULL * 1000000ULL;

bool isSleepWakeup = false;
byte nvsSensorCounter = 0;
byte nvsGsmCounter = 0;


uint32_t unixTime = 1746345600;  // Platzhalter-Zeit bis erste Synchronisierung
uint32_t syncMillis = 0;  // Zeitpunkt der letzten Synchronisierung (millis)
uint32_t lastSendTime = 0;  // wird nach setup gesetzt
uint8_t minutesLeft = 10;
unsigned long lastMinuteTick = 0;


#include <arkcore_tsim7070g.h>
#include <aranet_rn.h>
#include <epaper.h>
#include <gsm_tsim7070g.h>

void switchResetReason() {
  esp_reset_reason_t reason = esp_reset_reason();

  switch (reason) {
    case ESP_RST_POWERON:
      // Serial.println("Reset-Grund: Power-On oder Flash-Vorgang");
      saveSensorDataToNVS();
      break;

    case ESP_RST_DEEPSLEEP:
      isSleepWakeup = true;
      loadCountersFromNVS();
      nvsSensorCounter++;
      nvsGsmCounter++; 
      // Serial.println("Reset-Grund: Aufwachen aus Deep Sleep");
      break;

    case ESP_RST_SW:
    case ESP_RST_WDT:
    case ESP_RST_PANIC:
    case ESP_RST_BROWNOUT:
      // Serial.println("Reset-Grund: Watchdog, Software-Reset, Panic oder Brownout");
      break;

    default:
      // Serial.println("Reset-Grund: Unbekannt oder sonstiger Grund");
      break;
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);  // Brownout-Detector deaktivieren
  delay(100);
  pinMode(0, INPUT);
  pinMode(2, INPUT);
  pinMode(12, INPUT); // blaue LED
  pinMode(13, INPUT);
  pinMode(15, INPUT);
  pinMode(35, INPUT); // Spannungsteiler
  pinMode(MODEM_STATUS, INPUT); //  nur lesen
  pinMode(MODEM_PWRKEY, INPUT);
  //pinMode(MODEM_PWRKEY, OUTPUT); // Modem PWRKEY
  //digitalWrite(MODEM_PWRKEY, HIGH);  // PWRKEY = HIGH = AUS !
  //pinMode(MODEM_PWRKEY, OUTPUT);
  //digitalWrite(MODEM_PWRKEY, LOW);


  Serial.begin(115200);
  delay(100);

  SN = generateSerialFromChipId();
  //SN = "RGZ9R"; // nur für mein Testmodell !!!!

  switchResetReason();
  if (!isSleepWakeup) {
    delay(1000);
    ePaperInit();
    loadSensorDataFromNVS();
    showTextInRegion("RADOCON", 3, 22);   
    showTextInRegion(SN.c_str(), 151, 22);
    blinkBlueLed3x();
    showTextInRegion("Reset", 56, 12);   
    delay(1000);   
    showTextInRegion("ALPHA", 83, 12);     
    showTextInRegion("FW V0.6", 110, 12);     
    delay(2000);
    testAndShutdownModem();
    blinkBlueLed3x();
    delay(8000);
  }

  Serial.println("");
  if (isSleepWakeup) {  
    Serial.println("Reset-Grund: Aufwachen aus Deep Sleep");
  } else {
    Serial.println("Reset-Grund: Anderer Grund");
  }
  
  // BLE Sensor Reading (nur alle 9 Minuten)
  if (!isSleepWakeup || (nvsSensorCounter == 9)) {
    scanSensor();
    delay(100);
    readSensor();
    if (humidity == 0) {
      loadSensorDataFromNVS();
    }
    //NimBLEDevice::deinit(true);
    saveSensorDataToNVS();
    nvsSensorCounter = 0;
  }



  loadSensorDataFromNVS();
  ePaperInit();

  // Display: FULL REFRESH nur bei BLE-Messung oder GSM-Senden
  //if (!isSleepWakeup || (nvsSensorCounter == 9) || (nvsGsmCounter == 54)) {
    
    drawScreen();  // Komplettes Display mit Sensordaten
    snprintf(printLine, sizeof(printLine), "%u Bq", radon);
    showTextInRegion(printLine, 35, 22);     
    showTextInRegion(SN.c_str(), 151, 22);
  //} 
  // else -> PARTIAL REFRESH (nur Countdown/Akku, kein drawScreen())

  // Batterie-Status (immer)
  measureBattery();
  if (batteryVolts100 == 500) {
    snprintf(printLine, sizeof(printLine), "Akku laden");
  } else if (batteryVolts100 < 330) {
    snprintf(printLine, sizeof(printLine), "Akku leer");
  } else {
    snprintf(printLine, sizeof(printLine), "Akku %.2fV", batteryVolts100 / 100.0);
  }
  showTextInRegion(printLine, 126, 12);

  // GSM Senden (nur bei guter Batterie und alle 54 Minuten)
  //batteryVolts100 = 320;
  if (batteryVolts100 > 330 && (!isSleepWakeup || nvsGsmCounter == 54)) {
    loadSensorDataFromNVS();
    pressure = batteryVolts100 + 400; // temporär batterie senden
    sendSensorDataViaGSM();
    nvsGsmCounter = 0;
  }

  // Countdown (immer)
  snprintf(printLine, sizeof(printLine), "Messen in %2um", 9-nvsSensorCounter);
  showTextInRegion(printLine, 78, 12);    
  snprintf(printLine, sizeof(printLine), "Senden in %2um", 54-nvsGsmCounter);
  showTextInRegion(printLine, 102, 12);

  display.hibernate();  // Display in tiefsten Schlafmodus
  SPI.end();

  Serial.println("Deep Sleep...");
  saveCountersToNVS();

  //digitalWrite(MODEM_PWRKEY, HIGH); // PWRKEY = HIGH = aus
  
  //pinMode(MODEM_PWRKEY, INPUT);        // entkoppeln
  //pinMode(MODEM_PWRKEY, OUTPUT);   // bewusst definieren
  //digitalWrite(MODEM_PWRKEY, HIGH); // halten
  delay(100);                      // stabilisieren
  
  //delay(54000); // Alternativer sleep mit viel verbrauch
  //esp_sleep_enable_timer_wakeup(1000000ULL); // 1 Sekunde Deep Sleep
  
  esp_sleep_enable_timer_wakeup(1 * sleepMinute);
  esp_deep_sleep_start();

  //lastSendTime = millis(); remember the variable you will need it
}

void loop() {}