#include <Arduino.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include "AsyncUDP.h"
#include <TFT_eSPI.h> // Include the graphics library
#include <MLX90393.h>
#include <CST816S.h>
#include "RotationManager.h"
#include "NetworkingManager.h"

RotationManager rotationManager;
NetworkingManager networkingManager;

TFT_eSPI tft = TFT_eSPI(); // Create object "tft"
TFT_eSprite sprite = TFT_eSprite(&tft);
#define arcOffsetAngle 40
#define dbMinOffset 6000
#define numVolumeArcs 3
#define TFTSIZE 240

#define dimmingPin 14

#define numBuses 3
#define numOutputs 3

// CST816S touch(21, 22, 33, 32); // sda, scl, rst, irq
CST816S touch(37, 38, 36, 35); // sda, scl, rst, irq
AsyncUDP Udp;
WiFiManager wifiManager;
unsigned int localPort = 6980;       // local port to listen on
IPAddress destIP(192, 168, 72, 235); // Voicemeeter Potato IP

enum UiState
{
  LOADING,
  DISCONNECTED,
  MONITOR,
  OUTPUTS
};

// put function declarations here:
bool getStripOutputEnabled(byte stripNo, byte outputNo);
void setDisplayBrightness(byte brightness, bool instant = false);
void handleNewUDPPacket(AsyncUDPPacket packet);
void drawUi(UiState currentState);
void handleTouches();

// #define VBAN_PROTOCOL_MASK 0xE0
// #define VBAN_PROTOCOL_SERVICE 0x60

// #define VBAN_SERVICE_RTPACKETREGISTER 32
// #define VBAN_SERVICE_RTPACKET 33

// char currentPacket[sizeof(tagVBAN_VMRT_PACKET) + 50]; // allow a little extra space
tagVBAN_VMRT_PACKET currentRTPPacket;
byte targetBrightness = 250;
UiState currentPage = LOADING;
struct pendingButton
{
  bool isPending = false;
  bool pendingState = false;
  unsigned long pendingTime = 0;
};
pendingButton pendingButtonGridState[numBuses * numOutputs] = {pendingButton()};

// return number between 0 and 6000
short getOutputLevel(byte channel)
{
  short val = currentRTPPacket.inputLeveldB100[channel] + dbMinOffset;
  if (val < 0)
    val = 0;
  return val;
}
short getStripLevel(byte channel)
{
  short val = currentRTPPacket.stripGaindB100Layer1[channel] + dbMinOffset;
  if (val < 0)
    val = 0;
  return val;
}
void drawVolumneArc(int offset, byte thickness, uint16_t stripColour, uint16_t outputColour, short stripLevel, short outputLevel1, short outputLevel2)
{
  uint16_t x = (tft.width()) / 2; // Position of centre of arc
  uint16_t y = (tft.height()) / 2;
  uint8_t radius = tft.width() / 2 - offset; // Outer arc radius
  uint8_t inner_radius = radius - thickness; // Calculate inner radius (can be 0 for circle segment)

  float stripPercent = float(stripLevel) / dbMinOffset;
  float stripEndAngle = stripPercent * (360 - 2 * arcOffsetAngle) + arcOffsetAngle;
  float output1EndAngle = (float(outputLevel1) / dbMinOffset) * stripPercent * (360 - 2 * arcOffsetAngle) + arcOffsetAngle;
  float output2EndAngle = (float(outputLevel2) / dbMinOffset) * stripPercent * (360 - 2 * arcOffsetAngle) + arcOffsetAngle;
  uint16_t dividerCol = tft.color565(64, 64, 64);

  sprite.drawArc(x, y, inner_radius + 1, inner_radius - 1, arcOffsetAngle, 360 - arcOffsetAngle, dividerCol, TFT_BLACK, true);

  // 0 degrees is at 6 o'clock position. Arcs are drawn clockwise from start_angle to end_angle
  if (stripEndAngle > arcOffsetAngle)
    sprite.drawSmoothArc(x, y, radius, inner_radius, arcOffsetAngle, stripEndAngle, stripColour, dividerCol, true);
  if (output1EndAngle > arcOffsetAngle)
    sprite.drawArc(x, y, radius, inner_radius + thickness / 2, arcOffsetAngle, output1EndAngle, outputColour, stripColour, false);
  if (output2EndAngle > arcOffsetAngle)
    sprite.drawArc(x, y, radius - thickness / 2, inner_radius, arcOffsetAngle, output2EndAngle, outputColour, stripColour, false);
}

void drawVolumeArcs(byte selected, short levels[9])
{
  short offset = 0;
  uint16_t fg_color = 0xaff3;

  for (byte i = 0; i < numVolumeArcs; i++)
  {
    if (selected == i)
    {
      drawVolumneArc(offset, 18, 0x5549, fg_color, levels[i * 3], levels[i * 3 + 1], levels[i * 3 + 2]);
      offset += 18;
    }
    else
    {
      drawVolumneArc(offset, 10, 0x4a49, 0x7bcf, levels[i * 3], levels[i * 3 + 1], levels[i * 3 + 2]);
      offset += 10;
    }
  }
}

#define buttonWidthSmall 30
#define buttonHeightSmall 20
#define gridGapSmall 5

#define buttonWidthLarge 60
#define buttonHeightLarge 60
#define gridGapLarge 10

#define screenCenter 120

void drawBusOutputSelectButtons(uint16_t fg_color, byte bWidth, byte bHeight, byte gGap)
{
  byte totalWidth = (bWidth * numBuses + gGap * (numBuses - 1));
  byte halfWidth = totalWidth / 2;
  byte topOfButtons = (screenCenter - ((numOutputs * bHeight) + (gGap * (numOutputs - 1))) / 2);

  for (byte i = 0; i < numBuses; i++)
  {
    int leftmostPoint = screenCenter - halfWidth + (i * (bWidth + gGap));
    for (byte j = 0; j < numOutputs; j++)
    {
      // handle pending changes. Show grey background if pending. Check pending state befre showing
      bool changePending = pendingButtonGridState[i * numOutputs + j].isPending;
      bool actualState = getStripOutputEnabled(i + 5, j);
      bool shouldShowPending = false;
      if (changePending)
      {
        if (actualState == pendingButtonGridState[i * numOutputs + j].pendingState || millis() - pendingButtonGridState[i * numOutputs + j].pendingTime > 8000)
          pendingButtonGridState[i * numOutputs + j].isPending = false;
        else
          shouldShowPending = true;
      }

      // actually show the buttons
      if (shouldShowPending)
      {
        sprite.fillRoundRect(leftmostPoint, topOfButtons + (j * (bHeight + gGap)), bWidth, bHeight, 5, TFT_DARKGREY);
        sprite.setTextColor(TFT_WHITE);
        sprite.drawCentreString("A" + String(j + 1), leftmostPoint + bWidth / 2 + 1, topOfButtons + (j * (bHeight + gGap)) + bHeight / 2 - 8, 2);
      }
      else if (actualState)
      {
        sprite.fillRoundRect(leftmostPoint, topOfButtons + (j * (bHeight + gGap)), bWidth, bHeight, 5, fg_color);
        sprite.setTextColor(TFT_BLACK);
        sprite.drawCentreString("A" + String(j + 1), leftmostPoint + bWidth / 2 + 1, topOfButtons + (j * (bHeight + gGap)) + bHeight / 2 - 8, 2);
      }
      else
      {
        sprite.drawRoundRect(leftmostPoint, topOfButtons + (j * (bHeight + gGap)), bWidth, bHeight, 5, fg_color);
        sprite.setTextColor(fg_color);
        sprite.drawCentreString("A" + String(j + 1), leftmostPoint + bWidth / 2 + 1, topOfButtons + (j * (bHeight + gGap)) + bHeight / 2 - 8, 2);
      }
    }
  }
}

void setup()
{
  pinMode(45, OUTPUT);
  digitalWrite(45, HIGH); // enable peripheral power
  pinMode(0, OUTPUT);
  digitalWrite(0, HIGH); // something something reset
  // delay(5000);
  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(dimmingPin, OUTPUT);
  setDisplayBrightness(100);

  rotationManager.begin();

  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3);
  tft.drawCentreString("dlbs", 120, 81, 4);
  tft.setTextSize(1);
  tft.drawCentreString("connecting...", 120, 150, 2);

  touch.begin();

  bool result = networkingManager.begin();
}

unsigned long lastRTPRequestTime = 0;
unsigned long lastFrameTime = 0;
unsigned long lastTouchTime = 0;
unsigned long lastRotationTime = 0;
unsigned long lastInteractionTime = 0;
byte selectedVolumeArc = 0;

#define ROTATION_ANGLE_TO_DB_CHANGE 9.0 // degrees

void loop()
{
  // Set the current UI state and variables
  if (!networkingManager.isConnected())
    currentPage = DISCONNECTED;
  else if (currentPage == DISCONNECTED)
    currentPage = MONITOR;

  handleTouches();

  float angleDiff = rotationManager.update();
  if (angleDiff != 0.0f && currentPage == MONITOR)
  {
    float dbChange = angleDiff / ROTATION_ANGLE_TO_DB_CHANGE;
    networkingManager.incrementVolume(selectedVolumeArc, dbChange);
  }
  // handleRotation();
  lastInteractionTime = max(lastTouchTime, lastRotationTime);

  if (millis() - lastFrameTime > 10)
  {
    drawUi(currentPage);
  }

  networkingManager.update();
  currentRTPPacket = networkingManager.getCurrentPacket();
}

void drawUi(UiState currentState)
{
  lastFrameTime = millis();
  uint16_t fg_color = 0xaff3;
  uint16_t bg_color = TFT_BLACK; // This is the background colour used for smoothing (anti-aliasing)
  sprite.setColorDepth(8);
  sprite.createSprite(TFTSIZE, TFTSIZE);
  sprite.fillSprite(bg_color);

  if (currentState == MONITOR)
  {
    short levels[9] = {getStripLevel(13), getOutputLevel(10), getOutputLevel(11), getStripLevel(14), getOutputLevel(18), getOutputLevel(19), getStripLevel(15), getOutputLevel(26), getOutputLevel(27)};
    drawVolumeArcs(selectedVolumeArc, levels);

    sprite.drawArc(120, 120, 120, 80, -arcOffsetAngle, arcOffsetAngle, bg_color, bg_color, false); // black out the bottom to get straight edges
    String dbString = String(currentRTPPacket.stripGaindB100Layer2[5 + selectedVolumeArc] / 100) + "dB";
    sprite.drawCentreString(dbString, 120, 200, 4);

    drawBusOutputSelectButtons(fg_color, buttonWidthSmall, buttonHeightSmall, gridGapSmall);
    if (millis() - lastInteractionTime < 5000) // TODO rewrite this to be stateful
      setDisplayBrightness(255, true);
    else
      setDisplayBrightness(40);
  }
  else if (currentState == OUTPUTS)
  {
    drawBusOutputSelectButtons(fg_color, buttonWidthLarge, buttonHeightLarge, gridGapLarge);
  }

  else if (currentState == DISCONNECTED)
  {
    sprite.drawCentreString("Disconnected", 120, 94, 4);
    sprite.drawCentreString(WiFi.localIP().toString(), 120, 120, 2);
    setDisplayBrightness(4);
  }

  sprite.pushSprite(0, 0);
  sprite.deleteSprite();
}

void handleButtonTouches(int x, int y)
{
  // Detect which button is being pressed
  int tpx = TFTSIZE - x;
  int tpy = TFTSIZE - y;
  // Work out which strip is being pressed (x axis)
  int buttonGridWidth = (buttonWidthLarge * numBuses + gridGapLarge * (numBuses - 1));
  int leftMostPoint = screenCenter - buttonGridWidth / 2;
  int rightMostPoint = screenCenter + buttonGridWidth / 2;
  if (tpx < leftMostPoint || tpx > rightMostPoint)
    return;
  if (tpy < leftMostPoint || tpy > rightMostPoint) // it's a square, cheating
    return;
  byte xIndex = (tpx - leftMostPoint) / (buttonGridWidth / numBuses);
  byte yIndex = (tpy - leftMostPoint) / (buttonGridWidth / numOutputs);
  bool newState = !getStripOutputEnabled(5 + xIndex, yIndex);
  String commandString = "Strip[" + String(5 + xIndex) + "].A" + String(yIndex + 1) + " = " + String(newState);
  Serial.println(commandString);
  networkingManager.sendCommand(commandString);

  // update the pending button infor
  pendingButtonGridState[xIndex * numOutputs + yIndex].isPending = true;
  pendingButtonGridState[xIndex * numOutputs + yIndex].pendingState = newState;
  pendingButtonGridState[xIndex * numOutputs + yIndex].pendingTime = millis();

  Serial.print("xIndex: ");
  Serial.println(xIndex);
}

bool inTouchPeriod = false;
int touchPeriodStartPoint[2];

void handleTouches()
{
  // if (touch.available() && millis() - lastTouchTime > 150)
  if (touch.available())
  {
    bool isFingerDown = touch.data.points;
    if (isFingerDown && !inTouchPeriod)
    {
      inTouchPeriod = true;
      touchPeriodStartPoint[0] = touch.data.x;
      touchPeriodStartPoint[1] = touch.data.y;
    }
    byte gestureId = touch.data.gestureID;
    if (touch.data.points == 0 && touch.data.event == 1 && gestureId == NONE)
    {
      // user took their finger off, but system didn't detect a gesture
      // Try and detect some more single clicks here
      inTouchPeriod = false;
      Serial.println("In last resort touch func");

      // get the distance from the start of the period to where we are now.
      int dx = touchPeriodStartPoint[0] - touch.data.x;
      int dy = touchPeriodStartPoint[1] - touch.data.y;
      int distance = sqrt(dx * dx + dy * dy);
      if (distance < 10)
        gestureId = SINGLE_CLICK;
    }

    if (gestureId != SINGLE_CLICK && millis() - lastTouchTime < 150) // slow down the repeated gestures
      return;

    if (gestureId != NONE)
    {
      lastTouchTime = millis();
      switch (gestureId)
      {

      case SWIPE_DOWN:
        if (currentPage == MONITOR)
          networkingManager.incrementVolume(selectedVolumeArc, false);
        break;
      case SWIPE_UP:
        if (currentPage == MONITOR)
          networkingManager.incrementVolume(selectedVolumeArc, true);
        break;
      case SWIPE_RIGHT:
        if (currentPage == MONITOR)
        {
          if (selectedVolumeArc == 0)
            selectedVolumeArc = numVolumeArcs - 1;
          else
            selectedVolumeArc--;
        }
        break;
      case SWIPE_LEFT:
        if (currentPage == OUTPUTS)
          currentPage = MONITOR;
        else if (currentPage == MONITOR)
        {
          if (selectedVolumeArc == numVolumeArcs - 1)
            selectedVolumeArc = 0;
          else
            selectedVolumeArc++;
        }
        break;
      case SINGLE_CLICK:
        if (currentPage == MONITOR)
          currentPage = OUTPUTS;
        else if (currentPage == OUTPUTS)
          handleButtonTouches(touch.data.x, touch.data.y);
        break;
      case DOUBLE_CLICK:
        break;
      case LONG_PRESS:
        // ESP.restart();
        Serial.println("Long press detected");
        break;
      default:
        break;
      }
    }
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

bool getStripOutputEnabled(byte stripNo, byte outputNo)
{
  unsigned long stripState = currentRTPPacket.stripState[stripNo];
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

byte currentBrightness = 0;
void setDisplayBrightness(byte brightness, bool instant)
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
    analogWrite(dimmingPin, currentBrightness);
  }
}
