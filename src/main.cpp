// Minimal main which uses the refactored controller
#include <Arduino.h>
#include "VoicemeeterController.h"

VoicemeeterController controller;

void setup()
{
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH); // enable peripheral power
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH); // something something reset
  Serial.begin(115200);
  controller.begin();
}

void loop()
{
  controller.update();
  delay(10);
}