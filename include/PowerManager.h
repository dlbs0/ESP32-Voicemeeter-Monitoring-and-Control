#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MAX1704X.h>

class PowerManager
{
public:
    PowerManager();
    void begin();
    float getBatteryPercentage();
    float getBatteryVoltage();
    int getChargeTime();
    bool isCharging();

private:
    Adafruit_MAX17048 maxlipo;
    unsigned long lastCheckTime = 0;
    float batteryVoltage = 0;
    float batteryPercentage = 0;
    float chargeRate = 0;
    int chargeTime = -1;
    void updateBatteryInfo();
    int updateRate = 6000; // update every minute
};