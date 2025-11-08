#pragma once
#include <Arduino.h>
#include <WiFiManager.h>
#include <AsyncUDP.h>
#include "VoicemeeterProtocol.h"

class NetworkManager
{
public:
    NetworkManager();
    bool begin();
    void update();
    bool isConnected() const { return connected && lastPacketTime > 0 && (millis() - lastPacketTime) < 5000; }
    const tagVBAN_VMRT_PACKET &getCurrentPacket() const { return currentRTPPacket; }
    void sendCommand(const String &command);
    void setVolume(uint8_t channel, float level);
    void incrementVolume(uint8_t channel, bool up);

private:
    static const unsigned int LOCAL_PORT = 6980;
    static const IPAddress DEST_IP;

    WiFiManager wifiManager;
    AsyncUDP udp;
    bool connected;
    unsigned long lastPacketTime;
    unsigned long lastRTPRequestTime;
    tagVBAN_VMRT_PACKET currentRTPPacket;
    uint8_t commandFrameCounter;
    uint32_t serviceFrameCounter;

    std::vector<uint8_t> createCommandPacket(const String &command);
    std::vector<uint8_t> createRTPPacket();
    void handleUDPPacket(AsyncUDPPacket packet);
};