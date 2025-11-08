#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include "VoicemeeterProtocol.h"

enum UiState
{
    LOADING,
    DISCONNECTED,
    MONITOR,
    OUTPUTS
};

class DisplayManager
{
public:
    DisplayManager();
    void begin();
    void updateDisplay(UiState state, const tagVBAN_VMRT_PACKET &rtPacket);
    void setBrightness(uint8_t brightness, bool instant = false);
    void setSelectedArc(uint8_t arc) { selectedVolumeArc = arc; }
    uint8_t getSelectedArc() const { return selectedVolumeArc; }

    // Mark a button as pending. The display will show a temporary state until the RTP packet confirms it.
    void markPendingButton(uint8_t busIndex, uint8_t outputIndex, bool pendingState);

private:
    static const uint16_t SCREEN_SIZE = 240;
    static const uint8_t ARC_OFFSET_ANGLE = 40;
    static const uint16_t DB_MIN_OFFSET = 6000;
    static const uint8_t NUM_VOLUME_ARCS = 3;
    static const uint8_t NUM_BUSES = 3;
    static const uint8_t NUM_OUTPUTS = 3;

    TFT_eSPI tft;
    TFT_eSprite sprite;
    uint8_t currentBrightness;
    uint8_t selectedVolumeArc;
    uint8_t dimmingPin;

    void drawVolumeArcs(uint8_t selected, short levels[9]);
    void drawVolumeArc(int offset, uint8_t thickness, uint16_t stripColour,
                       uint16_t outputColour, short stripLevel,
                       short outputLevel1, short outputLevel2);
    void drawBusOutputSelectButtons(uint16_t fgColor, uint8_t bWidth,
                                    uint8_t bHeight, uint8_t gGap,
                                    const tagVBAN_VMRT_PACKET &rtPacket);
    void drawMonitorScreen(const tagVBAN_VMRT_PACKET &rtPacket);
    void drawOutputsScreen(const tagVBAN_VMRT_PACKET &rtPacket);
    void drawDisconnectedScreen();

    struct PendingButton
    {
        bool isPending = false;
        bool pendingState = false;
        unsigned long pendingTime = 0;
    };

    // layout: [busIndex * NUM_OUTPUTS + outputIndex]
    PendingButton pendingButtons[NUM_BUSES * NUM_OUTPUTS];
};