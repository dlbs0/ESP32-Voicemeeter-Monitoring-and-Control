#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "RotationManager.h"
#include "NetworkingManager.h"
#include "DisplayManager.h"
#include "PowerManager.h"

RotationManager rotationManager;
DisplayManager displayManager;
NetworkingManager networkingManager;
PowerManager powerManager;
tagVBAN_VMRT_PACKET currentRTPPacket;

unsigned long lastInteractionTime = 0;

#define ROTATION_ANGLE_TO_DB_CHANGE 9.0 // degrees

void setup()
{
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH); // something something reset

  // pinMode(45, OUTPUT);
  // digitalWrite(45, HIGH); // enable peripheral power
  Wire1.setPins(8, 9);

  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0)
  {
    lastInteractionTime = 1;
  }
  // else
  //   delay(5000);

  Serial.begin(115200);
  Serial.println("Starting...");
  Serial.printf("Initial millis: %lu\n", millis());
  powerManager.begin(&rotationManager);
  Serial.printf("PowerManager initialized. Millis: %lu\n", millis());
  networkingManager.setupStores();
  rotationManager.begin();
  Serial.printf("RotationManager initialized. Millis: %lu\n", millis());
  displayManager.begin(&powerManager, networkingManager.getDestIP());
  Serial.printf("DisplayManager initialized. Millis: %lu\n", millis());
  networkingManager.begin();
  Serial.printf("Setup complete. Millis: %lu\n", millis());
  displayManager.showIpAddress(networkingManager.getDeviceIP());
}

void loop()
{
  // Cap this loop to ~60 FPS to reduce CPU usage and allow idle
  const TickType_t loopFrequency = pdMS_TO_TICKS(16); // ~16 ms -> ~60Hz
  static TickType_t xLastWakeTime = 0;
  if (xLastWakeTime == 0)
    xLastWakeTime = xTaskGetTickCount();
  vTaskDelayUntil(&xLastWakeTime, loopFrequency);

  displayManager.setConnectionStatus(networkingManager.isConnected());

  networkingManager.update();
  currentRTPPacket = networkingManager.getCurrentPacket();
  displayManager.showLatestVoicemeeterData(currentRTPPacket);
  float batteryPercentage = powerManager.getBatteryPercentage();
  int chargeTime = powerManager.getChargeTime();
  displayManager.showLatestBatteryData(batteryPercentage, chargeTime, powerManager.getBatteryVoltage());

  auto currentScreen = displayManager.getCurrentScreen();

  float angleDiff = rotationManager.update();
  if (angleDiff != 0.0f && currentScreen == MONITOR)
  {
    float dbChange = angleDiff / ROTATION_ANGLE_TO_DB_CHANGE;
    networkingManager.incrementVolume(displayManager.getSelectedVolumeArc(), dbChange);
  }

  auto uiCommands = displayManager.getIssuedCommands();
  for (const auto &cmd : uiCommands)
  {
    networkingManager.sendCommand(cmd);
  }

  lastInteractionTime = max(displayManager.getLastTouchTime(), rotationManager.getLastRotationTime());
  powerManager.updateDisplayPowerState(networkingManager.getLastPacketTime(), lastInteractionTime, networkingManager.getConectionStartTime());
}
