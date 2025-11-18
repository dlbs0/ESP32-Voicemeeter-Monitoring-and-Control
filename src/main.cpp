#include <Arduino.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include "AsyncUDP.h"
#include <TFT_eSPI.h> // Include the graphics library
#include <MLX90393.h>
#include <CST816S.h>
#include "RotationManager.h"
#include "NetworkingManager.h"
#include "DisplayManager.h"
#include <lvgl.h>

RotationManager rotationManager;
DisplayManager displayManager;
NetworkingManager networkingManager;
tagVBAN_VMRT_PACKET currentRTPPacket;

unsigned long lastFrameTime = 0;
unsigned long lastInteractionTime = 0;

#define ROTATION_ANGLE_TO_DB_CHANGE 9.0 // degrees

void setup()
{
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH); // enable peripheral power
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH); // something something reset
  // delay(5000);
  Serial.begin(115200);
  Serial.println("Starting...");

  rotationManager.begin();
  displayManager.begin();

  bool result = networkingManager.begin();
}

void loop()
{
  displayManager.setConnectionStatus(networkingManager.isConnected());

  networkingManager.update();
  currentRTPPacket = networkingManager.getCurrentPacket();
  displayManager.showLatestVoicemeeterData(currentRTPPacket);

  if (millis() - lastFrameTime > 10)
    displayManager.update();
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
}
