#include <Arduino.h>
#include "utilities.h"
#include "pmu_tsim7080g.h"
#include "gsm_tsim7080g.h"

void setup()
{
    delay(2000);
    Serial.begin(115200);
    delay(2000);
    Serial.println();
    delay(2000);

    initPMU();
    ledBlinkFast();
    delay(4000);
    
    initGSM();
    ledAlwaysOn();
    enableModemPower();
    powerOnModem();

    delay(5000);
    Serial.println("");

    modem.poweroff();
    disableModemPower();
    ledBlinkSlow();
}

void loop()
{
    delay(5000);
    ledAlwaysOff();
}