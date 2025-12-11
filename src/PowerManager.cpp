#include "PowerManager.h"
#define WIRE Wire

byte PowerManager::currentBrightness = 1;

PowerManager::PowerManager()
{
}

void PowerManager::begin()
{
    pinMode(DIMMING_PIN, OUTPUT);
    setBrightness(0, true);

    if (!maxlipo.begin(&Wire1))
    {
        Serial.println("MAX17048 not found!");
    }
    // maxlipo.quickStart();
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
    return chargeRate > 0 || batteryPercentage >= 100.0f;
    // return true;
}

void PowerManager::updateDisplayPowerState(unsigned long lastNetworkActive, unsigned long lastUserInteraction, unsigned long connectionStartTime)
{
    if (lastCheckTime == 0 || millis() - lastCheckTime > updateRate)
        updateBatteryInfo();

    if (lastCheckTime == 0)
        return;
    // set up the internal variables with the latest data
    isPluggedIn = isCharging();
    emptyBattery = (batteryPercentage < 5.0f);

    bool tempDisplayOn = true;
    byte tempDisplayBrightness = 255;
    bool tempReducedFramerate = false;
    bool tempShouldDeepSleep = false;

    if (isPluggedIn)
    {
        if ((millis() - lastNetworkActive > PLUGGED_NO_NETWORK_TIMEOUT) && (millis() - lastUserInteraction > PLUGGED_NO_INTERACTION_TIMEOUT))
            tempDisplayOn = false;
    }
    else
    {
        if (millis() - lastUserInteraction > BATTERY_NO_INTERACTION_TIMEOUT)
        {
            tempReducedFramerate = true;
            tempDisplayBrightness = 30;

            if (millis() - lastNetworkActive > BATTERY_NO_NETWORK_TIMEOUT)
            {
                tempDisplayOn = false;
                tempReducedFramerate = true;
                tempShouldDeepSleep = true;
            }
        }
        if (millis() - connectionStartTime < 10000)
        {
            tempReducedFramerate = false;
        }

        if (emptyBattery)
        {
            tempShouldDeepSleep = true;
            tempDisplayOn = false;
        }
    }

    // if (!displayReady)
    //     tempDisplayBrightness = 0;
    if (!tempDisplayOn)
        tempDisplayBrightness = 0;

    displayOn = tempDisplayOn;
    displayBrightness = tempDisplayBrightness;
    reducedFramerate = tempReducedFramerate;
    shouldDeepSleep = tempShouldDeepSleep;

    managePower();
}

void PowerManager::managePower()
{
    // Manage peripheral power

    setBrightness(displayBrightness, true);

    // Manage deep sleep
    if (shouldDeepSleep)
    {
        digitalWrite(45, LOW);
        esp_wifi_stop();
        esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_INTERVAL * 1000ULL);

        Serial.println("Entering deep sleep due to low battery...");
        delay(1000); // allow time for message to be sent
        esp_deep_sleep_start();
    }

    if (millis() - lastPowerDecisionReportTime < updateRate)
        return;
    Serial.printf("PowerManager: displayOn=%d, brightness=%d, reducedFramerate=%d, shouldDeepSleep=%d\n",
                  displayOn, displayBrightness, reducedFramerate, shouldDeepSleep);
    lastPowerDecisionReportTime = millis();
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

    if (batteryVoltage != 0 && !isnan(batteryVoltage))
        lastCheckTime = millis();
}

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