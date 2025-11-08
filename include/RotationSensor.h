#pragma once
#include <Arduino.h>
#include <MLX90393.h>

// Small configurable wrapper used to init MLX90393 with a chosen I2C address
class MLX90393_Configurable : public MLX90393
{
private:
    uint8_t i2c_address;
    MLX90393Hal *hal_ = nullptr;

public:
    MLX90393_Configurable(uint8_t address = 0x18) : i2c_address(address) {}
    uint8_t begin_with_hal(MLX90393Hal *hal)
    {
        hal->set_address(i2c_address);
        uint8_t baseStatus = MLX90393::begin_with_hal(hal, 0, 0);
        hal->set_address(i2c_address);
        exit();
        uint8_t status1 = checkStatus(reset());
        uint8_t status2 = setGainSel(7);
        uint8_t status3 = setResolution(0, 0, 0);
        uint8_t status4 = setOverSampling(3);
        uint8_t status5 = setDigitalFiltering(7);
        uint8_t status6 = setTemperatureCompensation(0);
        return status1 | status2 | status3 | status4 | status5 | status6;
    }
};

class RotationSensor
{
public:
    RotationSensor(uint8_t address, TwoWire &wire);
    void begin();
    void update();
    bool hasNewRotation() const { return newRotationAvailable; }
    float getRotationDelta();

private:
    static constexpr float ANGLE_DEADBAND = 3.0f; // degrees
    static constexpr float ROTATION_TO_DB = 9.0f; // degrees per dB

    MLX90393_Configurable mlx;
    MLX90393ArduinoHal arduinoHal;
    float lastAngle;
    float currentAngle;
    bool newRotationAvailable;
    volatile bool dataReady;

    static void IRAM_ATTR dataReadyISR();
    void configureInterrupt(uint8_t intPin);
};