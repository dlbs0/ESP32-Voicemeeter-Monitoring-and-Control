#include "VoicemeeterController.h"
#include <Arduino.h>

VoicemeeterController::VoicemeeterController()
    : network(), display(), touch(37, 38, 36, 35), rotation(0x18, Wire1), currentState(LOADING), lastInteractionTime(0), displayTaskHandle(nullptr) {}

void VoicemeeterController::begin()
{
    Serial.println("Controller starting");
    display.begin();
    touch.begin();
    rotation.begin();
    network.begin();

    // start display task on dedicated core
    xTaskCreatePinnedToCore(VoicemeeterController::displayTask, "displayTask", 4 * 1024, this, 1, &displayTaskHandle, DISPLAY_TASK_CORE);
}

void VoicemeeterController::displayTask(void *parameter)
{
    auto *self = reinterpret_cast<VoicemeeterController *>(parameter);
    if (self)
        self->displayLoop();
    vTaskDelete(NULL);
}

void VoicemeeterController::displayLoop()
{
    while (true)
    {
        // update state based on network
        if (!network.isConnected())
            currentState = DISCONNECTED;
        else if (currentState == DISCONNECTED)
            currentState = MONITOR;

        const tagVBAN_VMRT_PACKET &pkt = network.getCurrentPacket();
        display.updateDisplay(currentState, pkt);
        delay(20);
    }
}

void VoicemeeterController::update()
{
    network.update();

    // touch handling
    TouchEvent ev = touch.getEvent();
    if (ev.isValid)
        handleTouch(ev);

    // rotation handling
    rotation.update();
    if (rotation.hasNewRotation())
        handleRotation();
}

void VoicemeeterController::handleTouch(const TouchEvent &event)
{
    lastInteractionTime = millis();
    switch (event.gestureId)
    {
    case SWIPE_DOWN:
        if (currentState == MONITOR)
            network.incrementVolume(display.getSelectedArc(), false);
        break;
    case SWIPE_UP:
        if (currentState == MONITOR)
            network.incrementVolume(display.getSelectedArc(), true);
        break;
    case SWIPE_RIGHT:
        if (currentState == MONITOR)
        {
            uint8_t sel = display.getSelectedArc();
            if (sel == 0)
                sel = 2;
            else
                sel--;
            display.setSelectedArc(sel);
        }
        break;
    case SWIPE_LEFT:
        if (currentState == OUTPUTS)
            currentState = MONITOR;
        else if (currentState == MONITOR)
        {
            uint8_t sel = display.getSelectedArc();
            if (sel == 2)
                sel = 0;
            else
                sel++;
            display.setSelectedArc(sel);
        }
        break;
    case SINGLE_CLICK:
        if (currentState == MONITOR)
            currentState = OUTPUTS;
        else if (currentState == OUTPUTS)
        {
            // button mapping to toggle output and show pending state until confirmed by RTP packet
            int x = event.x;
            int y = event.y;
            // transform coordinates to original orientation
            int tpx = 240 - x;
            int tpy = 240 - y;
            int buttonGridWidth = (60 * 3 + 10 * (3 - 1));
            int leftMostPoint = 120 - buttonGridWidth / 2;
            int rightMostPoint = 120 + buttonGridWidth / 2;
            if (tpx < leftMostPoint || tpx > rightMostPoint)
                return;
            if (tpy < leftMostPoint || tpy > rightMostPoint)
                return;
            uint8_t xIndex = (tpx - leftMostPoint) / (buttonGridWidth / 3);
            uint8_t yIndex = (tpy - leftMostPoint) / (buttonGridWidth / 3);

            // read actual state from latest RTP packet
            const tagVBAN_VMRT_PACKET &pkt = network.getCurrentPacket();
            unsigned long stripState = pkt.stripState[5 + xIndex];
            bool actualState = false;
            switch (yIndex)
            {
            case 0:
                actualState = (stripState & VMRTSTATE_MODE_BUSA1) != 0;
                break;
            case 1:
                actualState = (stripState & VMRTSTATE_MODE_BUSA2) != 0;
                break;
            case 2:
                actualState = (stripState & VMRTSTATE_MODE_BUSA3) != 0;
                break;
            default:
                actualState = false;
                break;
            }

            bool newState = !actualState;
            String commandString = "Strip[" + String(5 + xIndex) + "].A" + String(yIndex + 1) + " = " + String(newState);
            Serial.println(commandString);
            network.sendCommand(commandString);

            // mark pending in the display so user sees the change while we wait for RTP confirmation
            display.markPendingButton(xIndex, yIndex, newState);
        }
        break;
    case LONG_PRESS:
        Serial.println("Long press detected");
        break;
    default:
        break;
    }
}

void VoicemeeterController::handleRotation()
{
    float delta = rotation.getRotationDelta();
    if (delta == 0)
        return;
    if (currentState != MONITOR)
        return;
    display.setSelectedArc(display.getSelectedArc());
    network.setVolume(display.getSelectedArc(), delta);
}
