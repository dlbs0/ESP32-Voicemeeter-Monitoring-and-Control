#include <Arduino.h>
#include <WiFiManager.h> //https://github.com/tzapu/WiFiManager WiFi Configuration Magic
#include "AsyncUDP.h"
#include <TFT_eSPI.h> // Include the graphics library
#include <CST816S.h>

TFT_eSPI tft = TFT_eSPI(); // Create object "tft"
TFT_eSprite sprite = TFT_eSprite(&tft);
#define arcOffsetAngle 40
#define dbMinOffset 6000
#define numVolumeArcs 3
#define TFTSIZE 240

#define numBuses 3
#define numOutputs 3

CST816S touch(21, 22, 33, 32); // sda, scl, rst, irq
AsyncUDP Udp;
WiFiManager wifiManager;
unsigned int localPort = 6980; // local port to listen on
IPAddress destIP(192, 168, 72, 244);

enum UiState
{
  LOADING,
  DISCONNECTED,
  MONITOR,
  OUTPUTS
};

// put function declarations here:
std::vector<uint8_t> createCommandPacket(String &command);
void sendCommand(String command);
void incrementVolume(byte channel, bool up);
std::vector<uint8_t> createRTPPacket();
void printVector(std::vector<uint8_t> vec);
bool getStripOutputEnabled(byte stripNo, byte outputNo);
void setDisplayBrightness(byte brightness, bool instant = false);
void handleNewUDPPacket(AsyncUDPPacket packet);
void drawUi(UiState currentState);
void handleTouches();
/*
    VOICEMEETER POTATO STRIP/BUS INDEX ASSIGNMENT
    | Strip 1 | Strip 2 | Strip 2 | Strip 2 | Strip 2 |Virtual Input|Virtual AUX|   VAIO3   |BUS A1|BUS A2|BUS A3|BUS A4|BUS A5|BUS B1|BUS B2|BUS B3|
    +---------+---------+---------+---------+---------+-------------+-----------+-----------+------+------+------+------+------+------+------+------+
    |    0    |    1    |    2    |    3    |    4    |      5      |     6     |     7     |   0  |   1  |   2  |   3  |   4  |   5  |   6  |   7  |

  VOICEMEETER POTATO CHANNEL ASSIGNMENT

    INPUTS
    | Strip 1 | Strip 2 | Strip 3 | Strip 4 | Strip 5 |             Virtual Input             |            Virtual Input AUX          |                 VAIO3                 |
    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    | 00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 25 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 |

    OUTPUTS
    |             Output A1                 |                Output A2              |                Output A3              |                Output A4              |                Output A5              |
    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    | 00 | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | 20 | 21 | 22 | 23 | 24 | 25 | 26 | 27 | 28 | 29 | 30 | 31 | 32 | 33 | 34 | 35 | 36 | 37 | 38 | 39 |

    |            Virtual Output B1          |             Virtual Output B2         |             Virtual Output B3         |
    +----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+----+
    | 40 | 41 | 42 | 43 | 44 | 45 | 46 | 47 | 48 | 49 | 50 | 51 | 52 | 53 | 54 | 55 | 56 | 57 | 58 | 59 | 60 | 61 | 62 | 63 |
*/

typedef struct tagVBAN_VMRT_PACKET
{
  unsigned char magic[4]; // VBAN
  unsigned char subProtocol;
  unsigned char function;
  unsigned char service;        // 32 = VBAN_SERVICE_RTPACKETREGISTER
  unsigned char additionalInfo; //
  unsigned char streamName[16]; // stream name
  unsigned long frameCounter;
  // unsigned char reserved;           // needs to take up 19 bytes
  unsigned char voicemeeterType;    // 1 = Voicemeeter, 2= Voicemeeter Banana, 3 Potato
  unsigned char reserved;           // unused
  unsigned short buffersize;        // main stream buffer size
  unsigned long voicemeeterVersion; // version like for VBVMR_GetVoicemeeterVersion() functino
  unsigned long optionBits;         // unused
  unsigned long samplerate;         // main stream samplerate
  short inputLeveldB100[34];        // pre fader input peak level in dB * 100
  short outputLeveldB100[64];       // bus output peak level in dB * 100
  unsigned long TransportBit;       // Transport Status
  unsigned long stripState[8];      // Strip Buttons Status (see MODE bits below)
  unsigned long busState[8];        // Bus Buttons Status (see MODE bits below)
  short stripGaindB100Layer1[8];    // Strip Gain in dB * 100
  short stripGaindB100Layer2[8];
  short stripGaindB100Layer3[8];
  short stripGaindB100Layer4[8];
  short stripGaindB100Layer5[8];
  short stripGaindB100Layer6[8];
  short stripGaindB100Layer7[8];
  short stripGaindB100Layer8[8];
  short busGaindB100[8];         // Bus Gain in dB * 100
  char stripLabelUTF8c60[8][60]; // Strip Label
  char busLabelUTF8c60[8][60];   // Bus Label
} T_VBAN_VMRT_PACKET, *PT_VBAN_VMRT_PACKET, *LPT_VBAN_VMRT_PACKET;

#define VMRTSTATE_MODE_BUSA1 0x00001000
#define VMRTSTATE_MODE_BUSA2 0x00002000
#define VMRTSTATE_MODE_BUSA3 0x00004000
#define VMRTSTATE_MODE_BUSA4 0x00008000
#define VMRTSTATE_MODE_BUSA5 0x00080000

#pragma pack(1)

struct tagVBAN_HEADER
{
  unsigned long vban;       // contains 'V' 'B', 'A', 'N'
  unsigned char format_SR;  // SR index (see SRList above)
  unsigned char format_nbs; // nb sample per frame (1 to 256)
  unsigned char format_nbc; // nb channel (1 to 256)
  unsigned char format_bit; // mask = 0x07 (see DATATYPE table below)
  char streamname[16];      // stream name
  unsigned long nuFrame;    // growing frame number
};

#pragma pack()

typedef struct tagVBAN_HEADER T_VBAN_HEADER;
typedef struct tagVBAN_HEADER *PT_VBAN_HEADER;
typedef struct tagVBAN_HEADER *LPT_VBAN_HEADER;

#define VBAN_PROTOCOL_MASK 0xE0
#define VBAN_PROTOCOL_SERVICE 0x60

#define VBAN_SERVICE_RTPACKETREGISTER 32
#define VBAN_SERVICE_RTPACKET 33

// char currentPacket[sizeof(tagVBAN_VMRT_PACKET) + 50]; // allow a little extra space
tagVBAN_VMRT_PACKET currentRTPPacket;
unsigned long lastLevelsRecievedTime = 0;
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

void handleNewUDPPacket(AsyncUDPPacket packet)
{
  LPT_VBAN_HEADER lpHeader;
  lpHeader = (LPT_VBAN_HEADER)packet.data();
  if ((packet.length() > sizeof(T_VBAN_HEADER)) && (lpHeader->vban == 0x4E414256)) // Hex value is NABV
  {
    unsigned char protocol = lpHeader->format_SR & VBAN_PROTOCOL_MASK;
    if (protocol == VBAN_PROTOCOL_SERVICE)
    {
      if (lpHeader->format_nbc == VBAN_SERVICE_RTPACKET)
      {
        // if it's a RTPacket
        if (packet.length() >= sizeof(T_VBAN_VMRT_PACKET))
        {
          memcpy(&currentRTPPacket, packet.data(), sizeof(tagVBAN_VMRT_PACKET));

          lastLevelsRecievedTime = millis();
        }
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Starting...");

  pinMode(5, OUTPUT);
  setDisplayBrightness(100);

  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(3);
  tft.drawCentreString("dlbs", 120, 81, 4);
  tft.setTextSize(1);
  tft.drawCentreString("connecting...", 120, 150, 2);

  touch.begin();

  wifiManager.autoConnect("VOLUME");

  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  if (Udp.listen(localPort))
  {
    Serial.println("UDP connected");
    Udp.onPacket(handleNewUDPPacket);
    currentPage = MONITOR;
  }
  else
  {
    Serial.println("UDP connection failed");
  }
}

unsigned long lastRTPRequestTime = 0;
unsigned long lastFrameTime = 0;
unsigned long lastTouchTime = 0;
byte selectedVolumeArc = 0;

void loop()
{
  // Sert the current UI state and variables
  if (millis() - lastLevelsRecievedTime > 5000)
    currentPage = DISCONNECTED;
  else if (currentPage == DISCONNECTED)
    currentPage = MONITOR;

  handleTouches();

  if (millis() - lastFrameTime > 10)
  {
    drawUi(currentPage);
  }

  if (millis() - lastRTPRequestTime > 10000 || lastRTPRequestTime == 0)
  {
    // need to keep sending requests for more realtime data
    std::vector<uint8_t> rtp_packet = createRTPPacket();
    Udp.writeTo(rtp_packet.data(), rtp_packet.size(), destIP, localPort);
    lastRTPRequestTime = millis();

    // sendCommand("strip(1).mute = 1");
    // sendCommand("Strip[5].A1 = 1");
    // sendCommand("System.KeyPress(\"MEDIAPAUSE / MEDIAPLAY\")");
  }
}

void drawUi(UiState currentState)
{
  lastFrameTime = millis();
  uint16_t fg_color = 0xaff3;
  uint16_t bg_color = TFT_BLACK; // This is the background colour used for smoothing (anti-aliasing)
  sprite.setColorDepth(8);
  sprite.createSprite(TFTSIZE, TFTSIZE);
  sprite.fillSprite(bg_color);

  if (currentPage == MONITOR)
  {
    short levels[9] = {getStripLevel(13), getOutputLevel(10), getOutputLevel(11), getStripLevel(14), getOutputLevel(18), getOutputLevel(19), getStripLevel(15), getOutputLevel(26), getOutputLevel(27)};
    drawVolumeArcs(selectedVolumeArc, levels);

    sprite.drawArc(120, 120, 120, 80, -arcOffsetAngle, arcOffsetAngle, bg_color, bg_color, false); // black out the bottom to get straight edges
    String dbString = String(currentRTPPacket.stripGaindB100Layer2[5 + selectedVolumeArc] / 100) + "dB";
    sprite.drawCentreString(dbString, 120, 200, 4);

    drawBusOutputSelectButtons(fg_color, buttonWidthSmall, buttonHeightSmall, gridGapSmall);
    if (millis() - lastTouchTime < 5000) // TODO rewrite this to be stateful
      setDisplayBrightness(255, true);
    else
      setDisplayBrightness(40);
  }
  else if (currentPage == OUTPUTS)
  {
    drawBusOutputSelectButtons(fg_color, buttonWidthLarge, buttonHeightLarge, gridGapLarge);
  }

  else if (currentPage == DISCONNECTED)
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
  sendCommand(commandString);

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
          incrementVolume(selectedVolumeArc, false);
        break;
      case SWIPE_UP:
        if (currentPage == MONITOR)
          incrementVolume(selectedVolumeArc, true);
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

void incrementVolume(byte channel, bool up)
{
  String command = "strip(" + String(channel + 5) + ").gain " + (up ? "+" : "-") + "= 3";
  sendCommand(command);
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
    analogWrite(5, currentBrightness);
  }
}

void sendCommand(String command)
{
  std::vector<uint8_t> commandPacket = createCommandPacket(command);
  Udp.writeTo(commandPacket.data(), commandPacket.size(), destIP, localPort);
  Serial.println("sent packet");
}

byte commandFrameCounter = 0;
std::vector<uint8_t> createCommandPacket(String &command)
{
  String streamName = "Command1";
  commandFrameCounter++;
  std::vector<uint8_t> mute_packet = {0x56, 0x42, 0x41, 0x4e, 0x40, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, commandFrameCounter, 0x00, 0x00, 0x00};
  for (int i = 0; i < 16; i++)
  {
    mute_packet[i + 8] = streamName[i];
  }
  mute_packet.insert(mute_packet.end(), command.begin(), command.end());

  // Serial.print("Mute packet: ");
  // printVector(mute_packet);
  // Serial.print("Size of mute_packet: ");
  // Serial.println(sizeof(mute_packet));
  return mute_packet;
}

uint32_t serviceFrameCounter = 0;

std::vector<uint8_t> createRTPPacket()
{
  // String streamName = "Command1";
  // String command = "strip(1).mute = 0";
  // commandFrameCounter++;
  // std::vector<uint8_t> rtp_packet = {0x56, 0x42, 0x41, 0x4e, 0x60, 0x00, 0x20, 0x0f, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x20, 0x52, 0x54, 0x50, serviceFrameCounter};
  // std::vector<uint8_t> rtp_packet = {0x56, 0x42, 0x41, 0x4e, 0x60, 0x00, 0x20, 0x0f, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x20, 0x52, 0x54, 0x50, 0x00, 0x59, 0x41, 0x00, 0, 0, 0, 0}; //working, but why
  std::vector<uint8_t> rtp_packet = {0x56, 0x42, 0x41, 0x4e, 0x60, 0x00, 0x20, 0x0f, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x20, 0x52, 0x54, 0x50, 0x01, 0x59, 0x41, 0, 0, 0, 0, 154};

  // Print the mute_packet
  // Serial.print("RTP packet: ");
  // printVector(rtp_packet);
  // Serial.print("Size of rtp_packet: ");
  // Serial.println(sizeof(rtp_packet));
  return rtp_packet;
}

void printVector(std::vector<uint8_t> vec)
{
  for (int i = 0; i < vec.size(); i++)
  {
    Serial.print(vec[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}