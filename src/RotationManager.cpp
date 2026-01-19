#include "RotationManager.h"

static RotationManager *g_rotation_instance = nullptr;

void IRAM_ATTR RotationManager::dataReadyISR()
{
    if (g_rotation_instance)
        g_rotation_instance->dataReady = true;
}

RotationManager::RotationManager() : mlx(), arduinoHal(), lastAngle(-100.0f), lastRotationTime(0), dataReady(false)
{
    arduinoHal.set_twoWire(&Wire1);
}

void RotationManager::begin()
{
    g_rotation_instance = this;
    attachInterrupt(digitalPinToInterrupt(INT_PIN), RotationManager::dataReadyISR, RISING);

    uint8_t status = mlx.begin_with_hal(&arduinoHal); // A1, A0
    mlx.reset();

    mlx.setGainSel(1);
    mlx.setResolution(17, 17, 16); // x, y, z
    mlx.setOverSampling(2);
    mlx.setDigitalFiltering(4);
    mlx.setBurstDataRate(0); // this number gets multiplied by 20ms to set the burst data rate
    mlx.startBurst(MLX90393::X_FLAG | MLX90393::Y_FLAG);

    initialized = true; // Mark as initialized after setup complete
}

float RotationManager::update()
{
    // Read magnetometer data if ready
    if (dataReady)
    {
        dataReady = false;
        MLX90393::txyzRaw data; // Structure to hold x, y, z data

        // Read the magnetometer data (this should be fast in burst mode)
        if (mlx.readMeasurement(MLX90393::X_FLAG | MLX90393::Y_FLAG, data))
        {
            int16_t x = static_cast<int16_t>(data.x);
            int16_t y = static_cast<int16_t>(data.y);
            float angle = atan2(-y, x);
            angle = angle * 180 / PI; // convert to degrees
            angle += 180;             // offset to 0-360 degrees

            if (numStartupSamples <= 4) // discard first few samples to allow settling
            {
                lastAngle = angle;
                numStartupSamples++;
                return 0.0f;
            }

            // Serial.printf("X: %d, Y: %d, Angle: %.2f\n", x, y, angle);

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

void RotationManager::enterWakeOnChangeMode()
{
    mlx.exit();
    uint8_t wocDiff;
    mlx.getWocDiff(wocDiff);
    if (wocDiff != 1)
        mlx.setWocDiff(1);    // Enable Wake-On-Change on difference
    mlx.setWOXYThreshold(10); // Set threshold for wake-on-change
    mlx.setBurstDataRate(32); // this number gets multiplied by 20ms to set the burst data rate

    mlx.startWakeOnChange(MLX90393::X_FLAG | MLX90393::Y_FLAG);
    delay(10);
    Serial.println("Entered Wake-On-Change mode.");
    esp_sleep_enable_ext0_wakeup((gpio_num_t)INT_PIN, HIGH);
}

void RotationManager::deepSleep()
{
    mlx.setBurstDataRate(64); // this number gets multiplied by 20ms to set the burst data rate
    mlx.exit();
    delay(10);
    mlx.reset();
    Serial.println("Entering rotation deep sleep...");
}