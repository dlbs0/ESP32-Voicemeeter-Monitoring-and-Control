#pragma once
#include <Arduino.h>
#include <MLX90393.h>
#include <Wire.h>

class MLX90393_Configurable : public MLX90393
{
private:
    uint8_t i2c_address;
    MLX90393Hal *hal_ = nullptr;

public:
    enum
    {
        WOC_DIFF_REG = 0x1,
        WOC_DIFF_MASK = 0x1000,
        WOC_DIFF_SHIFT = 12
    };
    MLX90393_Configurable(uint8_t address = 0x18)
        : i2c_address(address) {}

    uint8_t begin_with_hal(MLX90393Hal *hal)
    {
        hal->set_address(i2c_address);
        uint8_t baseStatus = MLX90393::begin_with_hal(hal, 0, 0);
        hal->set_address(i2c_address); // overwrite to desired address
        exit();

        uint8_t status1 = checkStatus(reset());
        uint8_t status2 = setGainSel(7);
        uint8_t status3 = setResolution(0, 0, 0);
        uint8_t status4 = setOverSampling(3);
        uint8_t status5 = setDigitalFiltering(7);
        uint8_t status6 = setTemperatureCompensation(0);

        return status1 | status2 | status3 | status4 | status5 | status6;
    }
    uint8_t setWocDiff(int8_t ext_trig)
    {
        uint16_t old_val;
        uint8_t status1 = readRegister(WOC_DIFF_REG, old_val);
        uint8_t status2 = writeRegister(WOC_DIFF_REG,
                                        (old_val & ~WOC_DIFF_MASK) |
                                            ((uint16_t(ext_trig) << WOC_DIFF_SHIFT) &
                                             WOC_DIFF_MASK));
        return checkStatus(status1) | checkStatus(status2);
    }

    uint8_t getWocDiff(uint8_t &ext_trig)
    {
        uint16_t reg_val;
        uint8_t status = readRegister(WOC_DIFF_REG, reg_val);
        ext_trig = (reg_val & WOC_DIFF_MASK) >> WOC_DIFF_SHIFT;
        return checkStatus(status);
    }
};

class RotationManager
{
public:
    RotationManager();
    void begin();
    float update();
    long getLastRotationTime() { return lastRotationTime; }
    void deepSleep();
    void enterWakeOnChangeMode();
    bool isInitialized() const { return initialized; }

private:
    static constexpr float ANGLE_DEADBAND = 3.0f; // degrees

    MLX90393_Configurable mlx;
    MLX90393ArduinoHal arduinoHal;
    float lastAngle;
    long lastRotationTime;
    bool initialized = false;
    static const int INT_PIN = 10; // (INT pin on MLX90393)
    volatile bool dataReady;
    short numStartupSamples = 0;

    static void IRAM_ATTR dataReadyISR();
    void configureInterrupt(uint8_t intPin);
};