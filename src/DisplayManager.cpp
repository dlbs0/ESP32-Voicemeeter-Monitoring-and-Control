#include "DisplayManager.h"
#include <Arduino.h>
#include <WiFi.h>

// Pins and sizes
static const uint8_t DIMMING_PIN = 14;

DisplayManager::DisplayManager()
    : tft(), sprite(&tft), currentBrightness(0), selectedVolumeArc(0), dimmingPin(DIMMING_PIN) {}

void DisplayManager::begin()
{
    pinMode(dimmingPin, OUTPUT);
    tft.init();
    tft.setRotation(2);
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(3);
    tft.drawCentreString("dlbs", 120, 81, 4);
    tft.setTextSize(1);
    tft.drawCentreString("connecting...", 120, 150, 2);
    setBrightness(100, true);
}

void DisplayManager::setBrightness(uint8_t brightness, bool instant)
{
    if (brightness != currentBrightness)
    {
        if (instant)
            currentBrightness = brightness;
        else
        {
            if (brightness > currentBrightness)
                currentBrightness++;
            else
                currentBrightness--;
        }
        analogWrite(dimmingPin, currentBrightness);
    }
}

static short clampLevel(short v)
{
    if (v < 0)
        return 0;
    return v;
}

void DisplayManager::updateDisplay(UiState state, const tagVBAN_VMRT_PACKET &rtPacket)
{
    sprite.setColorDepth(8);
    sprite.createSprite(SCREEN_SIZE, SCREEN_SIZE);
    sprite.fillSprite(TFT_BLACK);

    if (state == MONITOR)
    {
        short levels[9] = {clampLevel(rtPacket.stripGaindB100Layer1[13] + DB_MIN_OFFSET), clampLevel(rtPacket.outputLeveldB100[10] + DB_MIN_OFFSET), clampLevel(rtPacket.outputLeveldB100[11] + DB_MIN_OFFSET),
                           clampLevel(rtPacket.stripGaindB100Layer1[14] + DB_MIN_OFFSET), clampLevel(rtPacket.outputLeveldB100[18] + DB_MIN_OFFSET), clampLevel(rtPacket.outputLeveldB100[19] + DB_MIN_OFFSET),
                           clampLevel(rtPacket.stripGaindB100Layer1[15] + DB_MIN_OFFSET), clampLevel(rtPacket.outputLeveldB100[26] + DB_MIN_OFFSET), clampLevel(rtPacket.outputLeveldB100[27] + DB_MIN_OFFSET)};
        drawVolumeArcs(selectedVolumeArc, levels);

        sprite.drawArc(120, 120, 120, 80, -ARC_OFFSET_ANGLE, ARC_OFFSET_ANGLE, TFT_BLACK, TFT_BLACK, false);
        String dbString = String(rtPacket.stripGaindB100Layer2[5 + selectedVolumeArc] / 100) + "dB";
        sprite.drawCentreString(dbString, 120, 200, 4);

        drawBusOutputSelectButtons(0xaff3, 30, 20, 5, rtPacket);
    }
    else if (state == OUTPUTS)
    {
        drawBusOutputSelectButtons(0xaff3, 60, 60, 10, rtPacket);
    }
    else if (state == DISCONNECTED)
    {
        sprite.drawCentreString("Disconnected", 120, 94, 4);
        sprite.drawCentreString(WiFi.localIP().toString(), 120, 120, 2);
        setBrightness(4, true);
    }

    sprite.pushSprite(0, 0);
    sprite.deleteSprite();
}

void DisplayManager::drawVolumeArcs(uint8_t selected, short levels[9])
{
    int offset = 0;
    uint16_t fg_color = 0xaff3;

    for (uint8_t i = 0; i < NUM_VOLUME_ARCS; i++)
    {
        if (selected == i)
        {
            drawVolumeArc(offset, 18, 0x5549, fg_color, levels[i * 3], levels[i * 3 + 1], levels[i * 3 + 2]);
            offset += 18;
        }
        else
        {
            drawVolumeArc(offset, 10, 0x4a49, 0x7bcf, levels[i * 3], levels[i * 3 + 1], levels[i * 3 + 2]);
            offset += 10;
        }
    }
}

void DisplayManager::drawVolumeArc(int offset, uint8_t thickness, uint16_t stripColour, uint16_t outputColour, short stripLevel, short outputLevel1, short outputLevel2)
{
    uint16_t x = (tft.width()) / 2;
    uint16_t y = (tft.height()) / 2;
    uint8_t radius = tft.width() / 2 - offset;
    uint8_t inner_radius = radius - thickness;

    float stripPercent = float(stripLevel) / DB_MIN_OFFSET;
    float stripEndAngle = stripPercent * (360 - 2 * ARC_OFFSET_ANGLE) + ARC_OFFSET_ANGLE;
    float output1EndAngle = (float(outputLevel1) / DB_MIN_OFFSET) * stripPercent * (360 - 2 * ARC_OFFSET_ANGLE) + ARC_OFFSET_ANGLE;
    float output2EndAngle = (float(outputLevel2) / DB_MIN_OFFSET) * stripPercent * (360 - 2 * ARC_OFFSET_ANGLE) + ARC_OFFSET_ANGLE;
    uint16_t dividerCol = tft.color565(64, 64, 64);

    sprite.drawArc(x, y, inner_radius + 1, inner_radius - 1, ARC_OFFSET_ANGLE, 360 - ARC_OFFSET_ANGLE, dividerCol, TFT_BLACK, true);

    if (stripEndAngle > ARC_OFFSET_ANGLE)
        sprite.drawSmoothArc(x, y, radius, inner_radius, ARC_OFFSET_ANGLE, stripEndAngle, stripColour, dividerCol, true);
    if (output1EndAngle > ARC_OFFSET_ANGLE)
        sprite.drawArc(x, y, radius, inner_radius + thickness / 2, ARC_OFFSET_ANGLE, output1EndAngle, outputColour, stripColour, false);
    if (output2EndAngle > ARC_OFFSET_ANGLE)
        sprite.drawArc(x, y, radius - thickness / 2, inner_radius, ARC_OFFSET_ANGLE, output2EndAngle, outputColour, stripColour, false);
}

// New implementation that uses the rtPacket and pendingButtons to determine display state
void DisplayManager::drawBusOutputSelectButtons(uint16_t fgColor, uint8_t bWidth, uint8_t bHeight, uint8_t gGap, const tagVBAN_VMRT_PACKET &rtPacket)
{
    uint8_t totalWidth = (bWidth * NUM_BUSES + gGap * (NUM_BUSES - 1));
    uint8_t halfWidth = totalWidth / 2;
    int topOfButtons = (120 - ((NUM_OUTPUTS * bHeight) + (gGap * (NUM_OUTPUTS - 1))) / 2);

    for (uint8_t i = 0; i < NUM_BUSES; i++)
    {
        int leftmostPoint = 120 - halfWidth + (i * (bWidth + gGap));
        for (uint8_t j = 0; j < NUM_OUTPUTS; j++)
        {
            // handle pending changes. Show grey background if pending.
            bool changePending = pendingButtons[i * NUM_OUTPUTS + j].isPending;
            bool actualState = false;
            unsigned long stripState = rtPacket.stripState[5 + i];
            switch (j)
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

            bool shouldShowPending = false;
            if (changePending)
            {
                if (actualState == pendingButtons[i * NUM_OUTPUTS + j].pendingState || millis() - pendingButtons[i * NUM_OUTPUTS + j].pendingTime > 8000)
                {
                    pendingButtons[i * NUM_OUTPUTS + j].isPending = false;
                }
                else
                {
                    shouldShowPending = true;
                }
            }

            // draw
            if (shouldShowPending)
            {
                sprite.fillRoundRect(leftmostPoint, topOfButtons + (j * (bHeight + gGap)), bWidth, bHeight, 5, TFT_DARKGREY);
                sprite.setTextColor(TFT_WHITE);
                sprite.drawCentreString("A" + String(j + 1), leftmostPoint + bWidth / 2 + 1, topOfButtons + (j * (bHeight + gGap)) + bHeight / 2 - 8, 2);
            }
            else if (actualState)
            {
                sprite.fillRoundRect(leftmostPoint, topOfButtons + (j * (bHeight + gGap)), bWidth, bHeight, 5, fgColor);
                sprite.setTextColor(TFT_BLACK);
                sprite.drawCentreString("A" + String(j + 1), leftmostPoint + bWidth / 2 + 1, topOfButtons + (j * (bHeight + gGap)) + bHeight / 2 - 8, 2);
            }
            else
            {
                sprite.drawRoundRect(leftmostPoint, topOfButtons + (j * (bHeight + gGap)), bWidth, bHeight, 5, fgColor);
                sprite.setTextColor(fgColor);
                sprite.drawCentreString("A" + String(j + 1), leftmostPoint + bWidth / 2 + 1, topOfButtons + (j * (bHeight + gGap)) + bHeight / 2 - 8, 2);
            }
        }
    }
}

void DisplayManager::markPendingButton(uint8_t busIndex, uint8_t outputIndex, bool pendingState)
{
    if (busIndex >= NUM_BUSES || outputIndex >= NUM_OUTPUTS)
        return;
    uint8_t idx = busIndex * NUM_OUTPUTS + outputIndex;
    pendingButtons[idx].isPending = true;
    pendingButtons[idx].pendingState = pendingState;
    pendingButtons[idx].pendingTime = millis();
}
