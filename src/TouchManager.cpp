#include "TouchManager.h"
#include <Arduino.h>

TouchManager::TouchManager(uint8_t sda, uint8_t scl, uint8_t rst, uint8_t irq)
    : touch(sda, scl, rst, irq), inTouchPeriod(false), touchStartX(0), touchStartY(0), lastTouchTime(0) {}

void TouchManager::begin()
{
    touch.begin();
}

TouchEvent TouchManager::getEvent()
{
    TouchEvent ev{false, 0, 0, 0};
    if (!touch.available())
        return ev;

    bool isFingerDown = touch.data.points;
    if (isFingerDown && !inTouchPeriod)
    {
        inTouchPeriod = true;
        touchStartX = touch.data.x;
        touchStartY = touch.data.y;
    }

    uint8_t gestureId = touch.data.gestureID;
    if (touch.data.points == 0 && touch.data.event == 1 && gestureId == NONE)
    {
        inTouchPeriod = false;
        int dx = touchStartX - touch.data.x;
        int dy = touchStartY - touch.data.y;
        int distance = sqrt(dx * dx + dy * dy);
        if (distance < TOUCH_DISTANCE_THRESHOLD)
            gestureId = SINGLE_CLICK;
    }

    if (gestureId != NONE && (millis() - lastTouchTime) < TOUCH_MIN_INTERVAL)
    {
        // throttle
        return ev;
    }

    if (gestureId != NONE)
    {
        lastTouchTime = millis();
        ev.isValid = true;
        ev.gestureId = gestureId;
        ev.x = touch.data.x;
        ev.y = touch.data.y;
    }
    return ev;
}
