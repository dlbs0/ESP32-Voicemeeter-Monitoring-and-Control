#include "PowerManager.h"
#define WIRE Wire

PowerManager::PowerManager()
{
}

void PowerManager::begin()
{
    // WIRE.begin(8, 9);

    if (!maxlipo.begin(&Wire1))
    {
        Serial.println("MAX17048 not found!");
    }
}

float PowerManager::getBatteryPercentage()
{
    if (millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    return batteryPercentage;
}

float PowerManager::getBatteryVoltage()
{
    if (millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    return batteryVoltage;
}

int PowerManager::getChargeTime()
{
    if (millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    return chargeTime;
}

bool PowerManager::isCharging()
{
    // positive charge rate means charging
    return maxlipo.chargeRate() > 0;
}

void PowerManager::updateBatteryInfo()
{

    lastCheckTime = millis();

    batteryVoltage = maxlipo.cellVoltage();
    batteryPercentage = maxlipo.cellPercent();
    chargeRate = maxlipo.chargeRate();
    if (chargeRate == 0)
    chargeTime = -1; // unknown
    else
    chargeTime = static_cast<int>(abs(batteryPercentage / chargeRate));
    Serial.printf("Battery Percentage: %f. Charge Time: %d. Charge Rate: %f\n", batteryPercentage, chargeTime, chargeRate);
}

// double cell_percent = maxlipo.cellPercent();
// double charge_rate = maxlipo.chargeRate();
// double cell_voltage = maxlipo.cellVoltage();
// String voltage = "";
// if (cell_voltage < 3.2)
// {
//     display.setTextColor(GxEPD_RED);
//     voltage = "Batt Low - Please Charge!";
// }
// else
//     voltage = String(cell_percent, 0) + "% " + String(cell_voltage, 1) + "V";

// String voltage_2 = String(abs(cell_percent / charge_rate), 0) + " hrs";