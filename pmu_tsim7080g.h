#define XPOWERS_CHIP_AXP2101    // before XPowersLib.h
#include "XPowersLib.h"
XPowersPMU  PMU;                

void initPMU()
{
    if (PMU.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        Serial.println("PMU Init");
        PMU.disableTSPinMeasure();

        PMU.disableDC2();
        PMU.disableDC3();
        PMU.disableDC4();
        PMU.disableDC5();

        PMU.disableALDO1();
        PMU.disableALDO2();
        PMU.disableALDO3();
        PMU.disableALDO4();

        PMU.disableBLDO2();
        PMU.disableCPUSLDO();
        PMU.disableDLDO1();
        PMU.disableDLDO2();
    } else {
        Serial.println("PMU ERROR");
        while (true) {
            delay(5000);
        }
    }
}

void enableModemPower()
{
    Serial.println("PMU power on -> GSM");
    PMU.setDC3Voltage(3000);  // SIM7080G Modem main power channel
    PMU.enableDC3();
    delay(100);
}

void disableModemPower()
{
    Serial.println("PMU power off -> GSM");
    PMU.disableDC3();     // Hauptversorgung Modem aus
}

void ledAlwaysOn()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_ON);
}

void ledAlwaysOff()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_OFF);
}

void ledBlinkSlow()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_1HZ);
}

void ledBlinkFast()
{
    PMU.setChargingLedMode(XPOWERS_CHG_LED_BLINK_4HZ);
}
