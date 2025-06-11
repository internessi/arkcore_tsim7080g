/**
 * epaper.h
 * alles für das display
 */

#undef GxEPD2_ENABLE_DIAGNOSTIC
#define GxEPD2_ENABLE_DIAGNOSTIC 0
#define ENABLE_GxEPD2_GFX 0

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>  
#include <Fonts/FreeSansBold18pt7b.h>
#include <FreeSansBold12pt7b.h>
#include <bahnschrift22pt7b.h>


// 1.54'' EPD Module
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(GxEPD2_154_D67(/*CS=*/21, /*DC=*/22, /*RES=*/5, /*BUSY=*/39)); // GDEH0154D67 200x200, SSD1681

void ePaperInit()
{
  SPI.begin(18, -1, 23, 21);
  delay(100);
  display.init(115200, true, 50, false);
  display.setRotation(2);
  Serial.println("Display init done");
}

void drawScreen()
{
  char tempStr[10];
  snprintf(tempStr, sizeof(tempStr), "%.1f", temperature);  // z. B. "22.7"
  display.setFullWindow();
  display.setTextColor(GxEPD_BLACK);

  // zuerst Temperatur

  int16_t x = 2;
  int16_t y = 31;

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // Temperaturwert
    display.setFont(&bahnschrift22pt7b);
    display.setCursor(x, y);
    display.print(tempStr);

    // Breite der Ziffern berechnen
    int16_t tbx, tby;
    uint16_t tbw, tbh;
    display.getTextBounds(tempStr, x, y, &tbx, &tby, &tbw, &tbh);

    // Gradzeichen als kleines o
    display.setFont(&FreeSansBold12pt7b);
    display.setCursor(x + tbw + 4, y - 19);
    display.print("o");

    // C wieder in Bahnschrift
    display.setFont(&bahnschrift22pt7b);
    display.setCursor(x + tbw + 15, y);
    display.print("C");
  
  // humidity anzeigen
    char humStr[5];
    snprintf(humStr, sizeof(humStr), "%.0f", humidity);  // ohne Nachkomma, z. B. "43"

    display.setFont(&bahnschrift22pt7b);
    display.setTextColor(GxEPD_BLACK);
    display.setCursor(130, 32);  // links vom Tropfen
    display.print(humStr);

    int16_t hx = 188;       // horizontal zentriert
    int16_t hy = 22;        // vertikale Mitte

    // Tropfenkörper (Kreis unten)
    display.fillCircle(hx, hy, 11, GxEPD_BLACK);  // Radius 11 → Durchmesser 22

    // Tropfenspitze (nach oben, 1 px kleiner)
    display.fillTriangle(hx - 11, hy, hx + 11, hy, hx, hy - 21, GxEPD_BLACK);

    // % innen (weiß, zentriert)
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(hx - 6, hy + 4);
    display.print("%");

    display.setTextColor(GxEPD_BLACK);  // Farbe zurücksetzen

  } while (display.nextPage());
}


void showTextInRegion(const char* text, int16_t clearY, uint8_t fontSize)
{
  // Berechne clearH aus fontSize (verdoppelt)
  int16_t clearH = fontSize * 2;
  
  display.setPartialWindow(0, clearY, display.width(), clearH);
  
  // Font-Auswahl basierend auf fontSize Parameter
  if (fontSize == 9) {
    display.setFont(&FreeMonoBold9pt7b);
  } else if (fontSize == 12) {
    display.setFont(&FreeSansBold12pt7b);
  } else if (fontSize == 22) {
    display.setFont(&bahnschrift22pt7b);
  } else {
    display.setFont(&bahnschrift22pt7b);  // Default für unbekannte Werte
  }
  
  display.setTextColor(GxEPD_BLACK);

  int16_t tbx, tby; 
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);
  int16_t x = (display.width() - tbw) / 2 - tbx;
  int16_t y = clearY + (clearH - tbh) / 2 - tby;

  display.firstPage();
  do {
    display.fillRect(0, clearY, display.width(), clearH, GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(text);
  } while (display.nextPage());
}