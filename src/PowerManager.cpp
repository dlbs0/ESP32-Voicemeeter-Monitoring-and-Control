#include "PowerManager.h"
#define WIRE Wire

byte PowerManager::currentBrightness = 1;

PowerManager::PowerManager()
{
}

void PowerManager::begin()
{
    // WIRE.begin(8, 9);
    pinMode(DIMMING_PIN, OUTPUT);
    setBrightness(0, true);

    if (!maxlipo.begin(&Wire1))
    {
        Serial.println("MAX17048 not found!");
    }
}

float PowerManager::getBatteryPercentage()
{
    if (lastCheckTime == 0 || millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    return batteryPercentage;
}

float PowerManager::getBatteryVoltage()
{
    if (lastCheckTime == 0 || millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    return batteryVoltage;
}

int PowerManager::getChargeTime()
{
    if (lastCheckTime == 0 || millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    return chargeTime;
}

bool PowerManager::isCharging()
{
    // positive charge rate means charging
    return chargeRate > 0;
}

void PowerManager::updateDisplayPowerState(unsigned long lastNetworkActive, unsigned long lastUserInteraction)
{
    if (lastCheckTime == 0)
        return;
    // set up the internal variables with the latest data
    isPluggedIn = isCharging();
    emptyBattery = (batteryPercentage < 5.0f);

    displayOn = true;
    displayBrightness = 255;
    reducedFramerate = false;
    peripheralPowerOn = true;
    shouldDeepSleep = false;

    if (isPluggedIn)
    {
        if ((millis() - lastNetworkActive > PLUGGED_NO_NETWORK_TIMEOUT) || (millis() - lastUserInteraction > PLUGGED_NO_INTERACTION_TIMEOUT))
            displayOn = false;
    }
    else
    {
        if (millis() - lastUserInteraction > BATTERY_NO_INTERACTION_TIMEOUT)
        {
            reducedFramerate = true;
            displayBrightness = 30;

            if (millis() - lastNetworkActive > BATTERY_NO_NETWORK_TIMEOUT)
            {
                displayOn = false;
                reducedFramerate = true;
                peripheralPowerOn = false;
                shouldDeepSleep = true;
            }
        }

        if (emptyBattery)
        {
            shouldDeepSleep = true;
            displayOn = false;
            peripheralPowerOn = false;
        }
    }

    if (!displayReady)
        displayBrightness = 0;
    if (!displayOn)
        displayBrightness = 0;

    managePower();
}

void PowerManager::managePower()
{
    // Manage peripheral power
    // digitalWrite(45, peripheralPowerOn ? HIGH : LOW);

    setBrightness(displayBrightness, false);

    // Manage deep sleep
    // if (shouldDeepSleep)
    // {
    //     esp_wifi_stop();
    //     esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_INTERVAL * 1000ULL);

    //     Serial.println("Entering deep sleep due to low battery...");
    //     delay(1000); // allow time for message to be sent
    //     // esp_deep_sleep_start();
    // }
    static bool lastDisplayOn = true;

    if (millis() - lastPowerDecisionReportTime < updateRate && displayOn == lastDisplayOn)
        return;
    Serial.printf("PowerManager: displayOn=%d, brightness=%d, reducedFramerate=%d, peripheralPowerOn=%d, shouldDeepSleep=%d\n",
                  displayOn, displayBrightness, reducedFramerate, peripheralPowerOn, shouldDeepSleep);
    lastPowerDecisionReportTime = millis();
    lastDisplayOn = displayOn;
}

void PowerManager::updateBatteryInfo()
{
    batteryVoltage = maxlipo.cellVoltage();
    batteryPercentage = maxlipo.cellPercent();
    chargeRate = maxlipo.chargeRate();
    if (chargeRate == 0)
        chargeTime = -1; // unknown
    else
        chargeTime = static_cast<int>(abs(batteryPercentage / chargeRate));
    Serial.printf("Battery Percentage: %f. Charge Time: %d. Charge Rate: %f. Voltage: %f\n", batteryPercentage, chargeTime, chargeRate, batteryVoltage);

    if (batteryVoltage != 0 && chargeRate != 0)
        lastCheckTime = millis();
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

void PowerManager::setBrightness(uint8_t brightness, bool instant)
{
    if (brightness != currentBrightness)
    {
        if (instant)
            currentBrightness = brightness;
        else
        {
            if (brightness - currentBrightness > 0)
                currentBrightness++;
            else
                currentBrightness--;
        }
        analogWrite(DIMMING_PIN, currentBrightness);
    }
}