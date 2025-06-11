#define TINY_GSM_MODEM_SIM7070
#define MODEM_RX 26
#define MODEM_TX 27
#define MODEM_BAUD 115200
#define MODEM_PWRKEY 4
#define MODEM_POWER_ON -1
#define MODEM_RST -1
#define MODEM_STATUS 34
#define LED_GPIO 12
#define LED_OFF HIGH
#define SerialMon Serial

#include <TinyGsmClient.h>

HardwareSerial sim7070(1);
TinyGsm modem(sim7070);
bool serialBegun = false;

uint16_t waitForModemReady(uint16_t timeoutMs = 15000) {
  unsigned long start = millis();
  sim7070.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  while (millis() - start < timeoutMs) {
    if (modem.testAT()) {
      return millis() - start;
    }
    delay(200);
  }
  return 0;  // 0 = Fehler/Timeout
}

void setupModem() {
  pinMode(MODEM_PWRKEY, OUTPUT);
  digitalWrite(MODEM_PWRKEY, HIGH);
  delay(1000);
  digitalWrite(MODEM_PWRKEY, LOW);
  waitForModemReady();
}

void shutdownModem() {
  modem.sendAT("+CPOWD=1");  // sauberes Powerdown
  delay(2000);  // Zeit für Abschaltung
}

void resetModem() {
  shutdownModem();   // Erst ausschalten
  delay(3000);       // Sicherheitspause nach dem Ausschalten
  setupModem();      // Dann wie gehabt einschalten
}


void sendSensorDataViaGSM() {
  if (!serialBegun) {
    sim7070.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
    serialBegun = true;
  }
  setupModem(); 

  SerialMon.print("GSM: Start - ");
  snprintf(printLine, sizeof(printLine), "GSM:");
  showTextInRegion(printLine, 78, 12);    

  snprintf(printLine, sizeof(printLine), "senden...");
  showTextInRegion(printLine, 102, 12);
  //modem.restart();
  modem.sendAT("+CFUN=1");
  modem.sendAT("+CNMP=38");  // LTE only
  modem.sendAT("+CMNB=2");   // Auto mode
  modem.sendAT("+CGDCONT=1,\"IP\",\"iot.1nce.net\"");
  SerialMon.print("bereit OK - ");

  SerialMon.print("Netz ");
  for (int i = 0; i < 7; i++) {
    if (modem.waitForNetwork(10000)) break;
    SerialMon.print(".");
  }
  SerialMon.print(" OK - ");
  delay(3000); 

  if (!modem.gprsConnect("iot.1nce.net", "", "")) {
    SerialMon.println("GPRS Fehler");
    snprintf(printLine, sizeof(printLine), "GPRS Fehler!");
    showTextInRegion(printLine, 102, 12);
    shutdownModem(); // HINZUFÜGEN
    return;
  }
  SerialMon.print("GPRS OK - ");

  TinyGsmClient client(modem);
  SerialMon.print("HTTP ");
  if (!client.connect("con.radocon.de", 80)) {
    SerialMon.println("Fehler");
    snprintf(printLine, sizeof(printLine), "HTTP Fehler!");
    showTextInRegion(printLine, 102, 12);
    shutdownModem(); // HINZUFÜGEN
    return;
  }
  SerialMon.print("OK - ");

  uint32_t now = getCurrentUnixTime();
  String url = "/nt/srd.php?sn=" + SN +
               "&bqm3=" + String(radon) +
               "&tmp=" + String(temperature, 1) +
               "&hum=" + String(humidity, 1) +
               "&hpa=" + String(pressure) +
               "&epoch=" + String(now);

  client.print("GET " + url + " HTTP/1.1\r\nHost: con.radocon.de\r\nConnection: close\r\n\r\n");

  String response = ""; bool capture = false;
  unsigned long timeout = millis() + 10000;
  while (millis() < timeout && (client.connected() || client.available())) {
    while (client.available()) {
      char c = client.read();
      if (c == '#') { capture = true; response = ""; }
      else if (capture) response += c;
    }
  }

  client.stop();
  modem.sendAT("+CPOWD=1");
  SerialMon.print("GSM: Close - ");
  snprintf(printLine, sizeof(printLine), "senden ok");
  showTextInRegion(printLine, 102, 12);

  response.trim();
  uint32_t serverTime = response.toInt();
  if (serverTime > 1746363600) {
    unixTime = serverTime;
    syncMillis = millis();
    SerialMon.printf("SYNC: %lu\n", unixTime);
  } else {
    SerialMon.printf("SYNC: Ung\xC3\xBCltige Zeit: '%s'\n", response.c_str());
  }
   //showTextOnDisplay("      ");
   //modem.poweroff();
   delay(2000);  // warten auf Abschaltvorgang
    // Sicherstellen, dass PWRKEY auf HIGH bleibt
}

bool testAndShutdownModem() {
  sim7070.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(100);  // kurze Stabilisierung

  bool ok = modem.testAT(1000);  // Antwort auf AT?

  if (ok) {
    SerialMon.println("Modem antwortet auf AT");
    shutdownModem();
  } else {
    SerialMon.println("Keine Antwort vom Modem");
  }

  sim7070.end();                 // UART freigeben
  serialBegun = false;          // Status zurücksetzen
  return ok;
}
