#pragma once
#include <Arduino.h>
#include <MLX90393.h>
#include <Wire.h>

// Arduino HAL that always talks to one fixed I2C address, whatever the library asks for.
// The library derives the address from the A1/A0 pins (0x0C..0x0F) but this board's sensor
// is at 0x18; without this, the library's own begin sent ~10 commands to a nonexistent
// device at 0x0C, each failing slowly (about 270 ms of boot time).
class MLX90393FixedAddrHal : public MLX90393ArduinoHal
{
public:
    void set_fixed_address(uint8_t address) { fixedAddress_ = address; }

    bool transceive(const uint8_t *request, size_t request_size, uint8_t *response, size_t response_size) override
    {
        i2cPort_->beginTransmission(fixedAddress_);
        for (size_t i = 0; i < request_size; i++)
        {
            if (i2cPort_->write(request[i]) != 1)
                return false;
        }
        i2cPort_->endTransmission();
        if (i2cPort_->requestFrom(fixedAddress_, response_size) != response_size)
            return false;
        for (size_t i = 0; i < response_size; i++)
        {
            if (!i2cPort_->available())
                return false;
            response[i] = i2cPort_->read();
        }
        return true;
    }

private:
    uint8_t fixedAddress_ = 0x18;
};

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
    enum
    {
        BURST_DATA_RATE_REG = 0x1,
        BURST_DATA_RATE_MASK = 0b00111111,
        BURST_DATA_RATE_SHIFT = 0
    };
    MLX90393_Configurable(uint8_t address = 0x18)
        : i2c_address(address) {}

    uint8_t begin_with_hal(MLX90393Hal *hal)
    {
        hal->set_address(i2c_address);

        // The sensor may still be in burst / wake-on-change mode after a deep sleep wake,
        // so leave that mode before configuring it (EX command, same as exit()). hal_ in the
        // base class is private and only set by its begin_with_hal, so send it by hand here.
        uint8_t exitCmd = CMD_EXIT;
        uint8_t exitStatus = 0;
        hal->transceive(&exitCmd, 1, &exitStatus, 1);

        // The base begin_with_hal computes its address from A1/A0 (0x0C..0x0F) and can't reach
        // our 0x18. The HAL we're given (MLX90393FixedAddrHal) ignores that and always uses
        // i2c_address, so this single call resets and configures the real sensor.
        return MLX90393::begin_with_hal(hal, 0, 0);
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

    uint8_t getBurstDataRate(uint8_t &bdr)
    {
        uint16_t reg_val;
        uint8_t status = readRegister(BURST_DATA_RATE_REG, reg_val);
        bdr = (reg_val & BURST_DATA_RATE_MASK) >> BURST_DATA_RATE_SHIFT;
        return checkStatus(status);
    }

    uint8_t setBurstDataRate(uint8_t bdr)
    {
        uint16_t old_val;
        uint8_t status1 = readRegister(BURST_DATA_RATE_REG, old_val);
        uint8_t status2 = writeRegister(BURST_DATA_RATE_REG,
                                        (old_val & ~BURST_DATA_RATE_MASK) |
                                            ((uint16_t(bdr) << BURST_DATA_RATE_SHIFT) &
                                             BURST_DATA_RATE_MASK));
        return checkStatus(status1) | checkStatus(status2);
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
    MLX90393FixedAddrHal arduinoHal;
    float lastAngle;
    long lastRotationTime;
    bool initialized = false;
    static const int INT_PIN = 10; // (INT pin on MLX90393)
    volatile bool dataReady;
    short numStartupSamples = 0;

    static void IRAM_ATTR dataReadyISR();
    void configureInterrupt(uint8_t intPin);
};