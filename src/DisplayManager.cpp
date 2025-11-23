#include "DisplayManager.h"

// Static member definitions for DisplayManager (must be in a single translation unit)
uint8_t DisplayManager::currentBrightness = 0;
TFT_eSPI DisplayManager::tft = TFT_eSPI();
CST816S DisplayManager::touch = CST816S(37, 38, 36, 35); // sda, scl, rst, irq
tagVBAN_VMRT_PACKET DisplayManager::latestVoicemeeterData = {0};
long DisplayManager::lastTouchTime = 0;
short DisplayManager::selectedVolumeArc = 0;
bool DisplayManager::connectionStatus = false;
char DisplayManager::dbLabelText[16] = "--.-- dB";

lv_obj_t *DisplayManager::strip_arcs[numVolumeArcs] = {nullptr};
lv_obj_t *DisplayManager::level_arcs_l[numVolumeArcs] = {nullptr};
lv_obj_t *DisplayManager::level_arcs_r[numVolumeArcs] = {nullptr};
lv_obj_t *DisplayManager::label_db = nullptr;

DisplayManager::DisplayManager()
{
    // ctor intentionally empty; static members are already defined above
}

void DisplayManager::begin()
{
    pinMode(DIMMING_PIN, OUTPUT);
    setBrightness(0, true); // start at full brightness
    /* Initialize LVGL */
    lv_init();
    /* Set the tick callback */
    lv_tick_set_cb(my_tick);
    /* Initialize the display driver (TFT_eSPI backend helper) */
    auto disp = lv_tft_espi_create(240, 240, lv_buffer, BUF_SIZE);
    lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);

    ui_init();
    setupLvglVaribleReferences();

    touch.begin();
    setBrightness(255, true); // start at full brightness

    // Register LVGL input device for the CST816S touchscreen (LVGL v9 API)
    {
        lv_indev_t *touch_indev = lv_indev_create();
        if (touch_indev)
        {
            lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
            lv_indev_set_read_cb(touch_indev, lv_touch_read);
            lv_indev_set_display(touch_indev, disp);
        }
    }
}

void DisplayManager::update()
{
    auto currentlyActiveScreen = lv_disp_get_scr_act(lv_display_get_default());

    if (!connectionStatus)
    {
        if (currentlyActiveScreen != ui_Loading)
            lv_scr_load(ui_Loading);
        currentScreen = DISCONNECTED;
    }
    else if (currentlyActiveScreen != ui_OutputMatrix && currentlyActiveScreen != ui_Config) // should be outputs
    {
        lv_scr_load(ui_Monitor);
        currentScreen = MONITOR;
    }
    else if (currentlyActiveScreen == ui_OutputMatrix)
    {
        currentScreen = OUTPUTS;
    }

    if (currentlyActiveScreen == ui_Monitor)
    {
        updateArcs();
        updateOutputButtons(true);
        // Update dB label
        int dbValue = getStripLevel(13 + selectedVolumeArc);
        // Format dB into the persistent buffer and update the label only if text changed.
        char tmp[16];
        float db = convertLevelToDb(dbValue);
        // Use snprintf for bounds-safety and consistent formatting
        snprintf(tmp, sizeof(tmp), "%2.1fdB", db);
        if (strcmp(tmp, dbLabelText) != 0)
        {
            strncpy(dbLabelText, tmp, sizeof(dbLabelText));
            dbLabelText[sizeof(dbLabelText) - 1] = '\0';
            lv_label_set_text_static(ui_LabelArcLevel, dbLabelText);
        }
    }
    if (currentlyActiveScreen == ui_OutputMatrix)
    {
        updateOutputButtons(false);
    }

    lv_timer_handler(); // Update the UI-
}

void DisplayManager::showLatestVoicemeeterData(const tagVBAN_VMRT_PACKET &packet)
{
    latestVoicemeeterData = packet;
}
void ::DisplayManager::setConnectionStatus(bool connected)
{

    connectionStatus = connected;
}
void DisplayManager::updateArcs()
{
    short outputLevels[numVolumeArcs * 2] = {getOutputLevel(10), getOutputLevel(11), getOutputLevel(18), getOutputLevel(19), getOutputLevel(26), getOutputLevel(27)};

    // short offset = 0;
    // Monitor screen layout: Create arc sets
    for (int i = 0; i < numVolumeArcs; ++i)
    {
        bool is_selected = (i == selectedVolumeArc);
        // int base_radius = TFTSIZE / 2; // Base radius for the largest arc set
        // const short borderWidth = 2;
        // int radius = base_radius - offset;
        // if (is_selected)
        //     offset += 18;
        // else
        //     offset += 10;
        // offset += borderWidth;

        // lv_arc_set_value(ui_LevelArc1, (getStripLevel(13)));

        // lv_obj_set_size(strip_arcs[i], radius * 2, radius * 2);
        lv_arc_set_value(strip_arcs[i], (getStripLevel(13 + i)));
        // lv_obj_set_style_arc_width(strip_arcs[i], is_selected ? 18 : 10, LV_PART_MAIN);
        // lv_obj_set_style_arc_width(strip_arcs[i], is_selected ? 18 : 10, LV_PART_INDICATOR);
        // lv_style_set_arc_color(&style_arc_indicator, lv_color_hex(is_selected ? 0x70c399 : 0x5549));
        lv_obj_set_style_arc_color(strip_arcs[i], lv_color_hex(is_selected ? 0x70C399 : 0x44765C), LV_PART_INDICATOR);

        // Left monitor level arc (slightly larger than main)
        // lv_obj_set_size(level_arcs_l[i], (radius) * 2, (radius) * 2);
        lv_arc_set_value(level_arcs_l[i], (outputLevels[i * 2] * getStripLevel(13 + i) / dbMinOffset));
        // lv_obj_set_style_arc_width(level_arcs_l[i], is_selected ? 9 : 5, LV_PART_MAIN);
        // lv_obj_set_style_arc_width(level_arcs_l[i], is_selected ? 9 : 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(level_arcs_l[i], lv_color_hex(is_selected ? 0x92FFC8 : 0x529070), LV_PART_INDICATOR);

        // Right monitor level arc (slightly smaller than main)
        // lv_obj_set_size(level_arcs_r[i], (radius - (is_selected ? 9 : 5)) * 2, (radius - (is_selected ? 9 : 5)) * 2);
        lv_arc_set_value(level_arcs_r[i], (outputLevels[i * 2 + 1] * getStripLevel(13 + i) / dbMinOffset));
        // lv_obj_set_style_arc_width(level_arcs_r[i], is_selected ? 9 : 5, LV_PART_MAIN);
        // lv_obj_set_style_arc_width(level_arcs_r[i], is_selected ? 9 : 5, LV_PART_INDICATOR);
        lv_obj_set_style_arc_color(level_arcs_r[i], lv_color_hex(is_selected ? 0x92FFC8 : 0x529070), LV_PART_INDICATOR);
    }
}

void DisplayManager::updateOutputButtons(bool previewButtons)
{
    // get the states for the output buttons
    bool buttonStates[numBuses * numOutputs];
    for (int i = 0; i < numBuses; ++i)
    {
        for (int j = 0; j < numOutputs; ++j)
        {
            buttonStates[i * numOutputs + j] = getStripOutputEnabled(5 + i, j);
        }
    }
    // get the button container
    lv_obj_t *btnContainer = ui_OutputButtonContainer;
    if (previewButtons)
        btnContainer = ui_OutputButtonPreviewContainer;

    // iterate through buttons and update their states
    auto childCount = lv_obj_get_child_count(btnContainer);
    for (short i = 0; i < childCount; i++)
    {
        auto btn = lv_obj_get_child(btnContainer, i);
        bool enabled = buttonStates[i];
        if (enabled)
            lv_obj_add_state(btn, LV_STATE_CHECKED);
        else
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
    }
}

void DisplayManager::setupLvglVaribleReferences()
{ // assign UI arc widgets to the pre-declared arrays
    strip_arcs[0] = ui_LevelArc1;
    strip_arcs[1] = ui_LevelArc2;
    strip_arcs[2] = ui_LevelArc3;
    level_arcs_l[0] = ui_MonitorArcL1;
    level_arcs_l[1] = ui_MonitorArcL2;
    level_arcs_l[2] = ui_MonitorArcL3;
    level_arcs_r[0] = ui_MonitorArcR1;
    level_arcs_r[1] = ui_MonitorArcR2;
    level_arcs_r[2] = ui_MonitorArcR3;
    Serial.println("LVGL initialized");

    // get the button container
    auto btnContainer = ui_OutputButtonContainer;
    ;
    // iterate through buttons and create callbacks
    auto childCount = lv_obj_get_child_count(btnContainer);
    for (short i = 0; i < childCount; i++)
    {
        auto btn = lv_obj_get_child(btnContainer, i);
        lv_obj_add_event_cb(btn, output_btn_event_cb, LV_EVENT_CLICKED, this);
    }

    lv_obj_add_event_cb(ui_Monitor, ui_event_Monitor_Gesture, LV_EVENT_GESTURE, NULL);
    lv_obj_add_event_cb(ui_MonitorIncrementSelectedChannel, ui_event_Monitor_Gesture, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_MonitorDecrementSelectedChannel, ui_event_Monitor_Gesture, LV_EVENT_CLICKED, NULL);
}

void DisplayManager::ui_event_Monitor_Gesture(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_RIGHT)
    {
        selectedVolumeArc++;
        if (selectedVolumeArc >= numVolumeArcs)
            selectedVolumeArc = 0;
    }
    else if (event_code == LV_EVENT_GESTURE && lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_LEFT)
    {
        if (selectedVolumeArc == 0)
            selectedVolumeArc = numVolumeArcs - 1;
        else
            selectedVolumeArc--;
    }

    if (event_code == LV_EVENT_CLICKED)
    {
        lv_obj_t *target = (lv_obj_t *)lv_event_get_target(e);
        if (target == ui_MonitorIncrementSelectedChannel)
        {
            selectedVolumeArc++;
            if (selectedVolumeArc >= numVolumeArcs)
                selectedVolumeArc = 0;
        }
        else if (target == ui_MonitorDecrementSelectedChannel)
        {
            if (selectedVolumeArc == 0)
                selectedVolumeArc = numVolumeArcs - 1;
            else
                selectedVolumeArc--;
        }
    }
}

// Helper to find a button index from object pointer
bool DisplayManager::find_output_button(lv_obj_t *btn, int &busIdx, int &outIdx)
{
    auto btnContainer = ui_OutputButtonContainer;
    auto buttonCount = lv_obj_get_child_count(btnContainer);

    for (int i = 0; i < numBuses; ++i)
    {
        for (int j = 0; j < numOutputs; ++j)
        {
            if (i * numOutputs + j >= buttonCount)
                return false;
            auto btnCandidate = lv_obj_get_child(btnContainer, i * numOutputs + j);

            if (btnCandidate == btn)
            {
                busIdx = i;
                outIdx = j;
                return true;
            }
        }
    }
    return false;
}

// Button event handler for outputs grid
void DisplayManager::output_btn_event_cb(lv_event_t *e)
{
    // Retrieve DisplayManager instance pointer passed as user data when the callback was attached
    DisplayManager *self = static_cast<DisplayManager *>(lv_event_get_user_data(e));
    if (!self)
        return;

    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    int busIdx = -1, outIdx = -1;
    if (!find_output_button(btn, busIdx, outIdx))
        return;

    // toggle state
    bool newState = !getStripOutputEnabled(5 + busIdx, outIdx);
    String commandString = "Strip[" + String(5 + busIdx) + "].A" + String(outIdx + 1) + " = " + String(newState);
    Serial.println(commandString);
    self->sendCommandString(commandString);

    // update pending on the instance
    // self->pendingButtonGridState[busIdx * numOutputs + outIdx].isPending = true;
    // self->pendingButtonGridState[busIdx * numOutputs + outIdx].pendingState = newState;
    // self->pendingButtonGridState[busIdx * numOutputs + outIdx].pendingTime = millis();
}

/* LVGL input device read callback and helper state
   This reads the CST816S touch driver and reports pointer coordinates
   and press/release state to LVGL. Coordinates are converted to the
   same orientation used elsewhere (TFTSIZE - x/y). */
static bool g_touch_pressed = false;
static int g_touch_x = 0;
static int g_touch_y = 0;
bool inTouchPeriod = false;
int touchPeriodStartPoint[2];
// Wrapper with generic pointers to avoid parser issues with forward typedefs in some toolchains.
void DisplayManager::lv_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    // Poll hardware when available and update cached values
    if (touch.available())
    {
        // bool isFingerDown = touch.data.points;
        // if (isFingerDown && !inTouchPeriod)
        // {
        //     inTouchPeriod = true;
        //     touchPeriodStartPoint[0] = touch.data.x;
        //     touchPeriodStartPoint[1] = touch.data.y;
        // }
        byte gestureId = touch.data.gestureID;
        // if (touch.data.points == 0 && touch.data.event == 1 && gestureId == NONE)
        // {
        //     // user took their finger off, but system didn't detect a gesture
        //     // Try and detect some more single clicks here
        //     inTouchPeriod = false;
        //     // Serial.println("In last resort touch func");

        //     // get the distance from the start of the period to where we are now.
        //     int dx = touchPeriodStartPoint[0] - touch.data.x;
        //     int dy = touchPeriodStartPoint[1] - touch.data.y;
        //     int distance = sqrt(dx * dx + dy * dy);
        //     if (distance < 10)
        //         gestureId = SINGLE_CLICK;
        // }

        // if (gestureId != SINGLE_CLICK && millis() - lastTouchTime < 100) // slow down the repeated gestures
        //     return;
        // else
        lastTouchTime = millis();

        g_touch_pressed = (touch.data.points > 0);
        g_touch_x = touch.data.x;
        g_touch_y = touch.data.y;

        // if (gestureId != NONE)
        // {
        //     auto currentlyActiveScreen = lv_disp_get_scr_act(lv_display_get_default());

        //     // lastTouchTime = millis();
        //     switch (gestureId)
        //     {

        //         // case SWIPE_DOWN:
        //         //   if (currentPage == MONITOR)
        //         //     incrementVolume(selectedVolumeArc, false);
        //         //   break;
        //         // case SWIPE_UP:
        //         //   if (currentPage == MONITOR)
        //         //     incrementVolume(selectedVolumeArc, true);
        //         //   break;

        //     case SWIPE_RIGHT:
        //         if (currentlyActiveScreen == ui_Monitor)
        //         {
        //             if (selectedVolumeArc == 0)
        //                 selectedVolumeArc = numVolumeArcs - 1;
        //             else
        //                 selectedVolumeArc--;
        //         }
        //         break;
        //     case SWIPE_LEFT:
        //         if (currentlyActiveScreen == ui_OutputMatrix)
        //             lv_scr_load(ui_Monitor);
        //         else if (currentlyActiveScreen == ui_Monitor)
        //         {
        //             if (selectedVolumeArc == numVolumeArcs - 1)
        //                 selectedVolumeArc = 0;
        //             else
        //                 selectedVolumeArc++;
        //         }
        //         break;

        //     // case SINGLE_CLICK:
        //     //   if (currentPage == MONITOR)
        //     //     currentPage = OUTPUTS;
        //     //   else if (currentPage == OUTPUTS)
        //     //     handleButtonTouches(touch.data.x, touch.data.y);
        //     //   break;
        //     case DOUBLE_CLICK:
        //         break;
        //     case LONG_PRESS:
        //         Serial.println("Long press detected");
        //         // lv_scr_load(ui_Config);

        //         // delay(500);
        //         // ESP.restart();
        //         break;
        //     default:
        //         break;
        //     }
        // }
        // if (gestureId == NONE)
        // {
        data->point.x = g_touch_x;
        data->point.y = g_touch_y;
        data->state = g_touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        // }

        Serial.print(touch.gesture());
        Serial.print("\t");
        Serial.print(touch.data.points);
        Serial.print("\t");
        Serial.print(touch.data.event);
        Serial.print("\t");
        Serial.print(touch.data.x);
        Serial.print("\t");
        Serial.println(touch.data.y);
    }
}

void DisplayManager::sendCommandString(const String &command)
{
    issuedCommands.push_back(command);
}

std::vector<String> DisplayManager::getIssuedCommands()
{
    std::vector<String> temp = issuedCommands;
    issuedCommands.clear();
    return temp;
}

// return number between 0 and 6000
short DisplayManager::getOutputLevel(byte channel)
{
    short val = latestVoicemeeterData.inputLeveldB100[channel] + dbMinOffset;
    if (val < 0)
        val = 0;
    return val;
}
short DisplayManager::getStripLevel(byte channel)
{
    short val = latestVoicemeeterData.stripGaindB100Layer1[channel] + dbMinOffset;
    if (val < 0)
        val = 0;
    return val;
}

float DisplayManager::convertLevelToPercent(int level)
{
    // level is 0 to dbMinOffset
    if (level <= 0)
        return 0.0f;
    if (level >= dbMinOffset)
        return 1.0f;
    return static_cast<float>(level) / dbMinOffset;
}

float DisplayManager::convertLevelToDb(int level)
{
    return (static_cast<float>(level) / 100.0f) - 60.0f;
}

bool DisplayManager::getStripOutputEnabled(byte stripNo, byte outputNo)
{
    unsigned long stripState = latestVoicemeeterData.stripState[stripNo];
    switch (outputNo)
    {
    case 0:
        return stripState & VMRTSTATE_MODE_BUSA1;
    case 1:
        return stripState & VMRTSTATE_MODE_BUSA2;
    case 2:
        return stripState & VMRTSTATE_MODE_BUSA3;
    case 3:
        return stripState & VMRTSTATE_MODE_BUSA4;
    case 4:
        return stripState & VMRTSTATE_MODE_BUSA5;
    default:
        return false;
    }
}

/* Tick source, tell LVGL how much time (milliseconds) has passed */
uint32_t DisplayManager::my_tick(void)
{
    return millis();
}

void DisplayManager::setBrightness(uint8_t brightness, bool instant)
{
    if (brightness != currentBrightness)
    {
        if (instant)
            currentBrightness = brightness;
        else
        {
            if (brightness - currentBrightness > 0)
                currentBrightness++;
            else
                currentBrightness--;
        }
        analogWrite(DIMMING_PIN, currentBrightness);
    }
}
