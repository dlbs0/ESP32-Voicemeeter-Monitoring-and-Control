#include "DisplayManager.h"
#include "PowerManager.h"

// Static member definitions for DisplayManager (must be in a single translation unit)
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

static QueueHandle_t s_cmdQueue = nullptr;
static TaskHandle_t s_displayTaskHandle = nullptr;

DisplayManager::DisplayManager()
{
    // ctor intentionally empty; static members are already defined above
}

void DisplayManager::begin()
{
    begin(nullptr);
}

void DisplayManager::begin(PowerManager *powerMgr, byte lastIPD)
{
    powerManager = powerMgr;
    lastIPDigit = lastIPD;
    usbSerialPreferences.begin("usbserial", false);

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
    lv_timer_handler(); // Update the UI

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

    // Mark initialization as complete BEFORE creating task
    isInitialized = true;

    // Run the UI functionality in a dedicated task
    if (!s_cmdQueue)
        s_cmdQueue = xQueueCreate(16, sizeof(NetworkCommand)); // capacity 16
    if (!s_displayTaskHandle)
    {
        xTaskCreatePinnedToCore(
            [](void *pv)
            {
                // Task entry: run LVGL handler loop
                DisplayManager *mgr = static_cast<DisplayManager *>(pv);
                for (;;)
                {
                    byte displayOn = 3;
                    byte lowFramerate = 3;
                    // Query power state from power manager
                    if (mgr->powerManager)
                    {
                        displayOn = mgr->powerManager->displayPowerOnRequired();
                        lowFramerate = mgr->powerManager->lowFramerateRequired();
                    }

                    // Only call LVGL APIs from this task
                    mgr->update(displayOn, lowFramerate);

                    // Dynamic refresh rate: 60Hz when active, 2Hz when idle
                    TickType_t delay = lowFramerate ? pdMS_TO_TICKS(250) : pdMS_TO_TICKS(16);
                    vTaskDelay(delay);
                }
            },
            "DisplayTask",
            12288, // increased stack size from 4096 -> 12288 (12KB) to avoid stack overflow
            this,
            2, // priority
            &s_displayTaskHandle,
            1 // pinned to core 1 (tune as needed)
        );
    }
}

void DisplayManager::update(byte displayShouldBeOn, byte reducePowerMode)
{
    // Safety check: don't attempt to update if display hasn't been initialized
    if (!isInitialized)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        return;
    }

    // FPS logging (optional)
    // static unsigned long fpsLastMs = 0;
    // static uint32_t fpsFrameCount = 0;
    // fpsFrameCount++;
    // unsigned long now = millis();
    // if (now - fpsLastMs >= 1000)
    // {
    //     Serial.print("Display FPS: ");
    //     Serial.println(fpsFrameCount);
    //     fpsFrameCount = 0;
    //     fpsLastMs = now;
    // }

    // Handle display sleep/wake based on power manager state
    // Use instance member to persist display state between calls
    if (displayShouldBeOn && !wasDisplayOn)
    {
        // Resume: turn display back on
        tft.writecommand(0x29); // TFT_DISPON: turn on display
        tft.writecommand(0x11); // TFT_SLPOUT: exit sleep mode
        Serial.printf("DisplayShouldBeOn: %d. WasDisplayOn: %d\n", displayShouldBeOn, wasDisplayOn);
        Serial.println("Display ON");
        wasDisplayOn = true;
    }
    else if (!displayShouldBeOn && wasDisplayOn)
    {
        // Shutdown: turn display off
        tft.writecommand(0x10); // TFT_SLPIN: enter sleep mode
        tft.writecommand(0x28); // TFT_DISPOFF: turn off display
        Serial.printf("DisplayShouldBeOn: %d. WasDisplayOn: %d\n", displayShouldBeOn, wasDisplayOn);
        Serial.println("Display OFF (sleep)");
        wasDisplayOn = false;
    }

    // When display is off, skip rendering to save power
    if (!displayShouldBeOn)
    {
        vTaskDelay(pdMS_TO_TICKS(250)); // minimal polling when display is off
        return;
    }

    // static lv_obj_t *lastLoadedScreen = nullptr;
    auto currentlyActiveScreen = lv_disp_get_scr_act(lv_display_get_default());

    // Choose a screen to display; only call lv_scr_load when target differs from current
    if (!connectionStatus)
    {
        if (currentlyActiveScreen != ui_Loading && currentlyActiveScreen != ui_Config)
        {
            lv_scr_load(ui_Loading); // create+load once
        }
        currentScreen = DISCONNECTED;
    }
    else if (currentlyActiveScreen != ui_OutputMatrix && currentlyActiveScreen != ui_Config)
    {
        if (currentlyActiveScreen == ui_Loading)
        {
            lv_obj_send_event(ui_Loading, LV_EVENT_KEY, NULL); // this will load the monitor screen with animation
        }
        else if (currentlyActiveScreen != ui_Monitor)
            lv_scr_load(ui_Monitor); // expensive call
        currentScreen = MONITOR;
    }
    else if (currentlyActiveScreen == ui_OutputMatrix)
    {
        currentScreen = OUTPUTS;
    }
    else if (currentlyActiveScreen == ui_Config)
    {
        currentScreen = CONFIG;
    }

    // Depending on chosen screen, update relevant elements
    if (currentlyActiveScreen == ui_Monitor)
    {
        updateArcs();
        updateOutputButtons(true);
    }
    else if (currentlyActiveScreen == ui_OutputMatrix)
    {
        updateOutputButtons(false);
    }
    else if (currentlyActiveScreen == ui_Config)
    {
        lv_label_set_text(ui_BatteryLifeLabel, (String(batteryPercentage) + "% " + String(chargeTime) + "h " + String(batteryVoltage) + "V").c_str());
    }
    else if (currentlyActiveScreen == ui_Loading)
    {
        static float lastBatteryLevel = -1;
        if (batteryPercentage != lastBatteryLevel)
        {
            lv_bar_set_value(ui_BatteryBar2, batteryPercentage, true);
            lastBatteryLevel = batteryPercentage;
        }
    }

    lv_timer_handler(); // Update the UI
    // powerManager->setDisplayReady(true);
    if (!hasSetupUSBSerial && millis() > 15000)
    {
        bool usbSerialEnabled = usbSerialPreferences.getBool("enabled", true);
        setUSBSerialEnabled(usbSerialEnabled);
        hasSetupUSBSerial = true;
    }
}
void DisplayManager::setUSBSerialEnabled(bool enabled)
{
    if (enabled)
    {
        Serial.println("USB Serial enabled from preferences.");
        Serial.begin(115200);
    }
    else
    {
        Serial.println("USB Serial disabled from preferences.");
        Serial.end();
    }
    usbSerialPreferences.putBool("enabled", enabled);
}

void DisplayManager::updateArcs()
{
    static int lastStripValue[numVolumeArcs] = {-1, -1, -1};
    static int lastLevelL[numVolumeArcs] = {-1, -1, -1};
    static int lastLevelR[numVolumeArcs] = {-1, -1, -1};
    static int lastSelectedArc = -1;
    static float lastBatteryLevel = -1;

    short outputLevels[numVolumeArcs * 2] = {getOutputLevel(10), getOutputLevel(11), getOutputLevel(18), getOutputLevel(19), getOutputLevel(26), getOutputLevel(27)};

    for (int i = 0; i < numVolumeArcs; ++i)
    {
        // Safety check: ensure arc objects are initialized
        if (!strip_arcs[i] || !level_arcs_l[i] || !level_arcs_r[i])
            continue;

        int stripVal = getStripLevel(13 + i);
        if (stripVal != lastStripValue[i])
        {
            lv_arc_set_value(strip_arcs[i], stripVal);
            lastStripValue[i] = stripVal;
        }

        int valL = (outputLevels[i * 2] * stripVal / dbMinOffset);
        if (valL != lastLevelL[i])
        {
            lv_arc_set_value(level_arcs_l[i], valL);
            lastLevelL[i] = valL;
        }

        int valR = (outputLevels[i * 2 + 1] * stripVal / dbMinOffset);
        if (valR != lastLevelR[i])
        {
            lv_arc_set_value(level_arcs_r[i], valR);
            lastLevelR[i] = valR;
        }

        // Only adjust colors/styles if selection changed
        if (selectedVolumeArc != lastSelectedArc)
        {
            bool is_selected = i == selectedVolumeArc;

            lv_obj_set_style_arc_color(strip_arcs[i], lv_color_hex(is_selected ? 0x70C399 : 0x44765C), LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(level_arcs_l[i], lv_color_hex(is_selected ? 0x92FFC8 : 0x529070), LV_PART_INDICATOR);
            lv_obj_set_style_arc_color(level_arcs_r[i], lv_color_hex(is_selected ? 0x92FFC8 : 0x529070), LV_PART_INDICATOR);
        }
    }
    lastSelectedArc = selectedVolumeArc;

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

    if (batteryPercentage != lastBatteryLevel)
    {
        lv_bar_set_value(ui_BatteryBar, batteryPercentage, true);
        lastBatteryLevel = batteryPercentage;
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

    // get the button container
    auto btnContainer = ui_OutputButtonContainer;
    // iterate through buttons and create callbacks
    auto childCount = lv_obj_get_child_count(btnContainer);
    for (short i = 0; i < childCount; i++)
    {
        auto btn = lv_obj_get_child(btnContainer, i);
        lv_obj_add_event_cb(btn, output_btn_event_cb, LV_EVENT_CLICKED, this);
    }

    lv_obj_add_event_cb(ui_Monitor, ui_event_Monitor_Callback, LV_EVENT_GESTURE, this);
    lv_obj_add_event_cb(ui_MonitorIncrementSelectedChannel, ui_event_Monitor_Callback, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_MonitorDecrementSelectedChannel, ui_event_Monitor_Callback, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_ResetButton, [](lv_event_t *e)
                        {
                            lv_event_code_t event_code = lv_event_get_code(e);
                            if (event_code == LV_EVENT_CLICKED)
                            {
                                Serial.println("Reset button clicked, restarting...");
                                delay(500);
                                ESP.restart();
                            } }, LV_EVENT_CLICKED, NULL);

    // Setup the config page

    lv_spinbox_set_value(ui_IPDigitsBox, lastIPDigit);
    lv_obj_add_event_cb(ui_IPDigitsBox, ui_event_IP_Change_Callback, LV_EVENT_VALUE_CHANGED, this);
    bool usbSerialEnabled = usbSerialPreferences.getBool("enabled", true);
    if (usbSerialEnabled)
        lv_obj_add_state(ui_USBSerialSwitch, LV_STATE_CHECKED);
    lv_obj_add_event_cb(ui_USBSerialSwitch, [](lv_event_t *e)
                        {
                            lv_event_code_t event_code = lv_event_get_code(e);
                            if (event_code == LV_EVENT_VALUE_CHANGED)
                            {
                                DisplayManager *self = static_cast<DisplayManager *>(lv_event_get_user_data(e));
                                if (!self)
                                    return;

                                bool state = lv_obj_has_state((lv_obj_t *)lv_event_get_target(e), LV_STATE_CHECKED);
                                self->setUSBSerialEnabled(state);
                            } }, LV_EVENT_VALUE_CHANGED, this);

    Serial.println("LVGL initialized");
}

void DisplayManager::ui_event_IP_Change_Callback(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    if (event_code == LV_EVENT_VALUE_CHANGED)
    {
        DisplayManager *self = static_cast<DisplayManager *>(lv_event_get_user_data(e));
        if (!self)
            return;

        byte lastIPDigits = lv_spinbox_get_value(ui_IPDigitsBox);
        if (lastIPDigits == self->lastIPDigit)
            return;

        self->sendCommandString(NetworkCommandType::SET_IP, String(lastIPDigits));
    }
}

void DisplayManager::ui_event_Monitor_Callback(lv_event_t *e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    // handle invisible buttons for increment/decrement on monitor screen
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
    else if (event_code == LV_EVENT_GESTURE)
    {
        if (lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_BOTTOM)
        {
            // send a play pause command through Voicemeeter
            DisplayManager *self = static_cast<DisplayManager *>(lv_event_get_user_data(e));
            if (self)
            {
                // self->sendCommandString(NetworkCommandType::SEND_VBAN_COMMAND, "System.KeyPress(\"MEDIAPAUSE / MEDIAPLAY\");");
                self->sendCommandString(NetworkCommandType::SEND_VBAN_COMMAND, "Command.Button[0].State = 1; Command.Button[0].State = 0; ");
            }
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
    self->sendCommandString(NetworkCommandType::SEND_VBAN_COMMAND, commandString);
}

// Wrapper with generic pointers to avoid parser issues with forward typedefs in some toolchains.
void DisplayManager::lv_touch_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    // Poll hardware when available and update cached values
    if (touch.available())
    {
        byte gestureId = touch.data.gestureID;
        lastTouchTime = millis();

        data->point.x = touch.data.x;
        data->point.y = touch.data.y;
        data->state = (touch.data.points > 0) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;

        // Serial.print(touch.gesture());
        // Serial.print("\t");
        // Serial.print(touch.data.points);
        // Serial.print("\t");
        // Serial.print(touch.data.event);
        // Serial.print("\t");
        // Serial.print(touch.data.x);
        // Serial.print("\t");
        // Serial.println(touch.data.y);
    }
}

void DisplayManager::showLatestVoicemeeterData(const tagVBAN_VMRT_PACKET &packet)
{
    latestVoicemeeterData = packet;
}
void DisplayManager::showLatestBatteryData(float battPerc, int chgTime, float battVolt)
{
    batteryPercentage = battPerc;
    chargeTime = chgTime;
    batteryVoltage = battVolt;
}

void DisplayManager::setConnectionStatus(bool connected)
{
    connectionStatus = connected;
}
void DisplayManager::setIsInteracting(bool interacting)
{
    isInteracting = interacting;
}

void DisplayManager::sendCommandString(NetworkCommandType type, const String &command)
{
    if (!s_cmdQueue)
    { /* fallback: ignore or use mutex */
        return;
    }
    NetworkCommand item;
    item.type = type;
    strncpy(item.payload, command.c_str(), sizeof(item.payload));
    item.payload[sizeof(item.payload) - 1] = '\0';
    xQueueSend(s_cmdQueue, &item, 0); // non-blocking; increase timeout if needed
}

std::vector<NetworkCommand> DisplayManager::getIssuedCommands()
{
    std::vector<NetworkCommand> out;
    if (!s_cmdQueue)
        return out;
    NetworkCommand item;
    while (xQueueReceive(s_cmdQueue, &item, 0) == pdTRUE)
    {
        out.emplace_back(item);
    }
    return out;
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

void DisplayManager::showIpAddress(uint32_t address)
{
    String ipStr =
        String(address & 0xFF) + "." +
        String((address >> 16) & 0xFF) + "." +
        String((address >> 8) & 0xFF) + ". ";
    lv_label_set_text(ui_IPAddress, ipStr.c_str());
}

/* Tick source, tell LVGL how much time (milliseconds) has passed */
uint32_t DisplayManager::my_tick(void)
{
    return millis();
}
