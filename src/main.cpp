#include <Arduino.h>
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

bool checkingForServer = true;

void setup()
{
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH); // something something reset
  delay(5000);
  Wire1.begin(8, 9, 10000000UL);

  Serial.begin(115200);
  Serial.println("Starting...");

  networkingManager.begin();
  powerManager.begin();
}

void loop()
{
  if (checkingForServer)
  {
    if (networkingManager.isConnected())
    {
      checkingForServer = false;
      Serial.println("Connected to Voicemeeter server.");
      pinMode(45, OUTPUT);
      digitalWrite(45, HIGH); // enable peripheral power

      displayManager.begin(&powerManager);
      rotationManager.begin();
    }
    networkingManager.update();
    powerManager.updateDisplayPowerState(networkingManager.getLastPacketTime(), 0, 0);
  }
  else
  {

    displayManager.setConnectionStatus(networkingManager.isConnected());

    networkingManager.update();
    currentRTPPacket = networkingManager.getCurrentPacket();
    displayManager.showLatestVoicemeeterData(currentRTPPacket);
    float batteryPercentage = powerManager.getBatteryPercentage();
    int chargeTime = powerManager.getChargeTime();
    displayManager.showLatestBatteryData(batteryPercentage, chargeTime, powerManager.getBatteryVoltage());

    // displayManager.update();
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
    // Update power manager with network and interaction state
    // bool hasUserInteraction = (millis() - lastInteractionTime) < 3000;
    // bool isNetworkActive = networkingManager.isConnected();
    powerManager.updateDisplayPowerState(networkingManager.getLastPacketTime(), lastInteractionTime, networkingManager.getConectionStartTime());
  }
}
