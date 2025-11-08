#pragma once
#include <Arduino.h>
#include <CST816S.h>

struct TouchEvent
{
    bool isValid;
    uint8_t gestureId;
    uint16_t x;
    uint16_t y;
};

class TouchManager
{
public:
    TouchManager(uint8_t sda, uint8_t scl, uint8_t rst, uint8_t irq);
    void begin();
    TouchEvent getEvent();

private:
    CST816S touch;
    bool inTouchPeriod;
    uint16_t touchStartX;
    uint16_t touchStartY;
    unsigned long lastTouchTime;

    static const int TOUCH_MIN_INTERVAL = 150; // ms
    static const int TOUCH_DISTANCE_THRESHOLD = 10;
};