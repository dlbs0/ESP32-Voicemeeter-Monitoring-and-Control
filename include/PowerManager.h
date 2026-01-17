#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX1704X.h>
#include <esp_wifi.h>
#include <RotationManager.h>

class PowerManager
{
public:
    PowerManager();
    void begin(RotationManager *rotMgr);
    float getBatteryPercentage();
    float getBatteryVoltage();
    int getChargeTime();
    bool isCharging();
    bool isEmptyBattery();

    // Display power management based on charging state and network activity
    void updateDisplayPowerState(unsigned long lastNetworkActive, unsigned long lastUserInteraction, unsigned long connectionStartTime);
    bool lowFramerateRequired() const { return reducedFramerate; }
    bool displayPowerOnRequired() const { return displayOn; }

    void setDisplayReady(bool ready) { displayReady = ready; }

    void deepSleep();

private:
    void updateBatteryInfo();
    void managePower();
    void setBrightness(uint8_t brightness, bool instant);

    Adafruit_MAX17048 maxlipo;
    RotationManager *rotationManager = nullptr;

    static const uint8_t DIMMING_PIN = 14;
    static uint8_t currentBrightness;
    unsigned long lastCheckTime = 0;
    unsigned long lastPowerDecisionReportTime = 0;
    float batteryVoltage = 0;
    float batteryPercentage = 0;
    float chargeRate = 0;
    int chargeTime = -1;

    int updateRate = 6000; // update every 6 seconds

    // Display power management
    volatile bool displayOn = true;
    byte displayBrightness = 255;
    volatile bool reducedFramerate = false;
    // bool peripheralPowerOn = true;
    bool shouldDeepSleep = false;

    bool isPluggedIn = false;
    volatile bool displayReady = false;

    // Timeouts for display power management
    static constexpr unsigned long BATTERY_NO_NETWORK_TIMEOUT = 16000;     // 2 minutes before display off on battery
    static constexpr unsigned long BATTERY_NO_INTERACTION_TIMEOUT = 20000; // 10 seconds before reducing framerate on battery
    static constexpr unsigned long PLUGGED_NO_NETWORK_TIMEOUT = 60000;     // 5 minutes without network packets before display off when plugged in
    static constexpr unsigned long PLUGGED_NO_INTERACTION_TIMEOUT = 60000; // 1 minute without any interaction when plugged in
    static constexpr unsigned long DEEP_SLEEP_INTERVAL = 12000000;         // 3 hours deep sleep on low battery
};