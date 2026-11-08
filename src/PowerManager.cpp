#include "PowerManager.h"
#define WIRE Wire

byte PowerManager::currentBrightness = 1;

// Battery state from the previous run, kept in RTC memory across deep sleep
RTC_DATA_ATTR static uint32_t rtcBatteryMagic = 0;
RTC_DATA_ATTR static float rtcBatteryVoltage = 0;
RTC_DATA_ATTR static float rtcBatteryPercentage = 0;
RTC_DATA_ATTR static float rtcChargeRate = 0;
static constexpr uint32_t RTC_BATTERY_MAGIC = 0xBA77E127;

PowerManager::PowerManager()
{
}

void PowerManager::begin(RotationManager *rotMgr)
{
    // Wire1.begin(8, 9);
    pinMode(DIMMING_PIN, OUTPUT);
    setBrightness(0, true);
    rotationManager = rotMgr;

    pinMode(45, OUTPUT);
    digitalWrite(45, HIGH); // enable peripheral power

    while (!maxlipo.begin(&Wire1))
    {
        Serial.println("MAX17048 not found!");
        delay(1);
    }
    Serial.println("MAX17048 found!");
    maxlipo.wake();

    if (restoreCachedBattery())
    {
        // Last run left the battery healthy, so don't block on the gauge (it needs a
        // while to produce a valid reading after begin() resets it). A real reading
        // replaces the cached values shortly after boot; if it turns out the battery
        // is empty, updateDisplayPowerState() will put us straight back to sleep.
        usedCache = true;
        Serial.printf("Using cached battery state (%.1f%%, %.2fV), skipping gauge wait\n", batteryPercentage, batteryVoltage);
        return;
    }

    float bv = maxlipo.cellVoltage();
    while (bv == 0 || isnan(bv))
    {
        Serial.println("Reading battery voltage...");
        delay(10);
        bv = maxlipo.cellVoltage();
    }
    Serial.println("Battery voltage: " + String(bv));
    updateBatteryInfo();
    if (isEmptyBattery())
    {
        Serial.println("Battery voltage too low! Deep sleeping...");
        deepSleep();
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
    // return false;
    return chargeRate > 0 || batteryPercentage >= 100.0f;
}

bool PowerManager::isEmptyBattery()
{
    // positive charge rate means charging
    return batteryVoltage < 3.65f;
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
            tempShouldDeepSleep = false;
            tempDisplayOn = true;
        }

        if (isEmptyBattery())
        {
            tempShouldDeepSleep = true;
            tempDisplayOn = false;
        }
    }

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

    setBrightness(displayBrightness, false);

    // Manage deep sleep
    if (shouldDeepSleep)
    {
        deepSleep();
    }

    if (millis() - lastPowerDecisionReportTime < updateRate)
        return;
    Serial.printf("PowerManager: displayOn=%d, brightness=%d, reducedFramerate=%d, shouldDeepSleep=%d\n",
                  displayOn, displayBrightness, reducedFramerate, shouldDeepSleep);
    lastPowerDecisionReportTime = millis();
}

void PowerManager::deepSleep()
{
    if (rotationManager != nullptr && rotationManager->isInitialized())
    {
        Serial.println("Preparing magnetometer for deep sleep...");
        if (isEmptyBattery())
            rotationManager->deepSleep();
        else
            rotationManager->enterWakeOnChangeMode();
    }
    digitalWrite(45, LOW);
    esp_wifi_stop();
    if (isEmptyBattery())
    {
        Serial.println("Entering deep sleep due to low battery (no wake source).");
        esp_deep_sleep_start();
    }
    // esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_INTERVAL * 100 * 1000ULL);

    Serial.println("Entering deep sleep (idle, wake on dial movement)...");
    delay(500); // allow time for message to be sent
    esp_deep_sleep_start();
}

void PowerManager::updateBatteryInfo()
{
    float voltage = maxlipo.cellVoltage();
    // The gauge reads 0/NaN until it has settled after a reset. Never let that
    // overwrite good (or cached) values, or we'd wrongly treat the battery as empty.
    if (voltage == 0 || isnan(voltage))
        return;

    batteryVoltage = voltage;
    batteryPercentage = maxlipo.cellPercent();
    chargeRate = maxlipo.chargeRate();
    if (chargeRate == 0)
        chargeTime = -1; // unknown
    else
        chargeTime = static_cast<int>(abs(batteryPercentage / chargeRate));
    Serial.printf("Battery Percentage: %f. Charge Time: %d. Charge Rate: %f. Voltage: %f\n", batteryPercentage, chargeTime, chargeRate, batteryVoltage);

    lastCheckTime = millis();

    // Remember for the next wake (RTC memory survives deep sleep)
    rtcBatteryVoltage = batteryVoltage;
    rtcBatteryPercentage = batteryPercentage;
    rtcChargeRate = chargeRate;
    rtcBatteryMagic = RTC_BATTERY_MAGIC;
}

bool PowerManager::restoreCachedBattery()
{
    if (rtcBatteryMagic != RTC_BATTERY_MAGIC)
        return false; // first boot / power loss: RTC memory is not valid
    if (rtcBatteryPercentage <= FAST_BOOT_MIN_PERCENT || isnan(rtcBatteryPercentage) || isnan(rtcBatteryVoltage))
        return false; // battery was getting low, take a real measurement before waking up

    batteryVoltage = rtcBatteryVoltage;
    batteryPercentage = rtcBatteryPercentage;
    chargeRate = rtcChargeRate;
    chargeTime = -1; // unknown until a real reading arrives

    // Mark as checked "now", but schedule the first real refresh ~3s after boot
    // (instead of a full updateRate) so a stale cache is corrected quickly.
    lastCheckTime = max(1UL, millis() - (unsigned long)(updateRate - 3000));
    return true;
}

void PowerManager::setBrightness(uint8_t brightness, bool instant)
{
    if (brightness != currentBrightness)
    {
        if (instant)
            currentBrightness = brightness;
        else
        {
            if (brightness > currentBrightness)
                currentBrightness = brightness;
            else if (brightness - currentBrightness > 0)
                currentBrightness++;
            else
                currentBrightness--;
        }

        analogWrite(DIMMING_PIN, currentBrightness);
    }
}