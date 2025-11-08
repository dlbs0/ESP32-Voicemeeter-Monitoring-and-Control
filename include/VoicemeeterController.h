#pragma once
#include <Arduino.h>
#include "NetworkManager.h"
#include "DisplayManager.h"
#include "TouchManager.h"
#include "RotationSensor.h"

class VoicemeeterController
{
public:
    VoicemeeterController();
    void begin();
    void update();

private:
    static const uint8_t DISPLAY_TASK_CORE = 0; // Graphics on core 0
    static const uint8_t MAIN_TASK_CORE = 1;    // Main logic on core 1

    NetworkManager network;
    DisplayManager display;
    TouchManager touch;
    RotationSensor rotation;

    UiState currentState;
    unsigned long lastInteractionTime;

    static void displayTask(void *parameter);
    void displayLoop();
    void handleTouch(const TouchEvent &event);
    void handleRotation();
    void updateBrightness();

    TaskHandle_t displayTaskHandle;
};