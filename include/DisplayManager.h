#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h> // Include the graphics library
#include <CST816S.h>
#include <lvgl.h>
#include "VoicemeeterProtocol.h"
#include "ui/ui.h"

#define dbMinOffset 6000
#define TFTSIZE 240
#define arcOffsetAngle 40
#define BUF_SIZE 240 * 240

#define numVolumeArcs 3
#define numBuses 3
#define numOutputs 3

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
    void update();
    static void setBrightness(uint8_t brightness, bool instant = false);
    void showLatestVoicemeeterData(const tagVBAN_VMRT_PACKET &packet);
    void showLatestBatteryData(float battPerc, int chgTime, float battVolt);
    void setConnectionStatus(bool connected);
    void setIsInteracting(bool interacting);
    std::vector<String> getIssuedCommands();
    long getLastTouchTime() { return lastTouchTime; }
    UiState getCurrentScreen() { return currentScreen; }
    short getSelectedVolumeArc() { return selectedVolumeArc; }

private:
    static const uint8_t DIMMING_PIN = 14;
    static uint8_t currentBrightness;
    static TFT_eSPI tft;
    static CST816S touch;
    static tagVBAN_VMRT_PACKET latestVoicemeeterData;
    static long lastTouchTime;
    static bool connectionStatus;
    static short selectedVolumeArc;
    UiState currentScreen = LOADING;
    void setupLvglVaribleReferences();
    void updateArcs();
    void updateOutputButtons(bool previewButtons);
    short getStripLevel(byte channel);
    short getOutputLevel(byte channel);
    float convertLevelToPercent(int level);
    float convertLevelToDb(int level);
    static bool getStripOutputEnabled(byte stripNo, byte outputNo);

    // initialse the buffer with all zeros
    uint8_t lv_buffer[BUF_SIZE] = {0};

    // For monitor: arc sets for each strip and outputs
    static lv_obj_t *strip_arcs[numVolumeArcs];   // Main volume arc
    static lv_obj_t *level_arcs_l[numVolumeArcs]; // Left channel level arc
    static lv_obj_t *level_arcs_r[numVolumeArcs]; // Right channel level arc
    static lv_obj_t *label_db;
    static char dbLabelText[16];

    // For outputs: grid buttons
    static lv_obj_t *output_buttons[numBuses][numOutputs];

    /* LVGL styles for arcs */
    static lv_style_t style_arc_main;      // Main strip volume arc background
    static lv_style_t style_arc_indicator; // Main strip volume indicator
    static lv_style_t style_arc_level_bg;  // Monitor level background
    static lv_style_t style_arc_level_ind; // Monitor level indicator

    static void lv_touch_read(lv_indev_t *indev, lv_indev_data_t *data);
    static void output_btn_event_cb(lv_event_t *e);
    static void ui_event_Monitor_Callback(lv_event_t *e);
    static bool find_output_button(lv_obj_t *btn, int &busIdx, int &outIdx);

    std::vector<String> issuedCommands;
    void sendCommandString(const String &command);

    static uint32_t my_tick(void);

    struct pendingButton
    {
        bool isPending = false;
        bool pendingState = false;
        unsigned long pendingTime = 0;
    };
    pendingButton pendingButtonGridState[numBuses * numOutputs] = {pendingButton()};

    float batteryPercentage = 0;
    float batteryVoltage = 0;
    int chargeTime = 0;

    bool isInteracting = false;
};