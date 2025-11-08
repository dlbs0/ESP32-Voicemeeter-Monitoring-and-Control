#include "RotationSensor.h"
#include <Wire.h>
#include <Arduino.h>

// Static pointer used by ISR
static RotationSensor *g_rotation_instance = nullptr;
static const int INT_PIN = 10;

void IRAM_ATTR RotationSensor::dataReadyISR()
{
    if (g_rotation_instance)
        g_rotation_instance->dataReady = true;
}

RotationSensor::RotationSensor(uint8_t address, TwoWire &wire)
    : mlx(address), arduinoHal(), lastAngle(-100.0f), currentAngle(0.0f), newRotationAvailable(false), dataReady(false)
{
    arduinoHal.set_twoWire(&wire);
}

void RotationSensor::begin()
{
    // configure sensor (Wire1 on ESP32)
    Wire1.begin(8, 9, 10000000UL);
    g_rotation_instance = this;
    attachInterrupt(digitalPinToInterrupt(INT_PIN), RotationSensor::dataReadyISR, RISING);

    mlx.begin_with_hal(&arduinoHal);
    mlx.reset();
    mlx.setGainSel(1);
    mlx.setResolution(17, 17, 16);
    mlx.setOverSampling(2);
    mlx.setDigitalFiltering(4);
    mlx.startBurst(MLX90393::X_FLAG | MLX90393::Y_FLAG | MLX90393::Z_FLAG);
}

void RotationSensor::update()
{
    if (!dataReady)
        return;
    dataReady = false;

    MLX90393::txyzRaw data;
    if (mlx.readMeasurement(MLX90393::X_FLAG | MLX90393::Y_FLAG | MLX90393::Z_FLAG, data))
    {
        int16_t x = static_cast<int16_t>(data.x);
        int16_t y = static_cast<int16_t>(data.y);
        int16_t z = static_cast<int16_t>(data.z);
        float angle = atan2(-y, x);
        angle = angle * 180 / PI;
        angle += 180;

        if (lastAngle == -100)
            lastAngle = angle;

        float angleDiff = angle - lastAngle;
        if (angleDiff > 180)
            angleDiff -= 360;
        if (angleDiff < -180)
            angleDiff += 360;

        if (abs(angleDiff) > ANGLE_DEADBAND)
        {
            lastAngle = angle;
            currentAngle = angleDiff;
            newRotationAvailable = true;
        }
    }
}

float RotationSensor::getRotationDelta()
{
    if (!newRotationAvailable)
        return 0.0f;
    newRotationAvailable = false;
    float dbChange = currentAngle / ROTATION_TO_DB;
    return dbChange;
}
