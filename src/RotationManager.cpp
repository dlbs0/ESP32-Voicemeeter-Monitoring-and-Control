#include "RotationManager.h"

static RotationManager *g_rotation_instance = nullptr;

void IRAM_ATTR RotationManager::dataReadyISR()
{
    if (g_rotation_instance)
        g_rotation_instance->dataReady = true;
}

RotationManager::RotationManager() : mlx(), arduinoHal(), lastAngle(0.0f), lastRotationTime(0), dataReady(false)
{
    arduinoHal.set_twoWire(&Wire1);
}

void RotationManager::begin()
{
    Wire1.begin(8, 9, 10000000UL);
    g_rotation_instance = this;
    attachInterrupt(digitalPinToInterrupt(INT_PIN), RotationManager::dataReadyISR, RISING);

    uint8_t status = mlx.begin_with_hal(&arduinoHal); // A1, A0
    mlx.reset();

    Serial.println(mlx.setGainSel(1));
    Serial.println(mlx.setResolution(17, 17, 16)); // x, y, z)
    Serial.println(mlx.setOverSampling(2));
    Serial.println(mlx.setDigitalFiltering(4));

    mlx.startBurst(MLX90393::X_FLAG | MLX90393::Y_FLAG | MLX90393::Z_FLAG);
}

float RotationManager::update()
{
    // Read magnetometer data if ready
    if (dataReady)
    {
        dataReady = false;
        MLX90393::txyzRaw data; // Structure to hold x, y, z data

        // Read the magnetometer data (this should be fast in burst mode)
        if (mlx.readMeasurement(MLX90393::X_FLAG | MLX90393::Y_FLAG | MLX90393::Z_FLAG, data))
        {
            int16_t x = static_cast<int16_t>(data.x);
            int16_t y = static_cast<int16_t>(data.y);
            int16_t z = static_cast<int16_t>(data.z);
            float angle = atan2(-y, x);
            angle = angle * 180 / PI; // convert to degrees
            angle += 180;             // offset to 0-360 degrees

            int arcAngle = angle - 90;
            if (arcAngle < 0)
                arcAngle += 360;

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
                lastRotationTime = millis();

                return angleDiff;
            }
        }
    }
    return 0.0f;
}