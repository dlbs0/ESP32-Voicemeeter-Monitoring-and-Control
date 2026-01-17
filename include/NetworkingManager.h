#pragma once
#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <Preferences.h>
#include "VoicemeeterProtocol.h"

enum NetworkCommandType
{
    SET_IP,
    SEND_VBAN_COMMAND
};

struct NetworkCommand
{
    NetworkCommandType type;
    char payload[64];
};

class NetworkingManager
{
public:
    NetworkingManager();
    void setupStores();
    bool begin();
    void update();
    bool isConnected() const { return connected; }
    const tagVBAN_VMRT_PACKET &getCurrentPacket() const { return currentRTPPacket; }
    void sendCommand(const NetworkCommand &command);
    void incrementVolume(uint8_t channel, bool up);
    void incrementVolume(uint8_t channel, float level);
    unsigned long getLastPacketTime() const { return lastPacketTime; }
    unsigned long getConectionStartTime() const { return connectionStartTime; }
    char getDestIP();
    uint32_t getDeviceIP();

private:
    static const unsigned int LOCAL_PORT = 6980;
    IPAddress DEST_IP;
    WiFiManager wifiManager;
    Preferences preferences;
    AsyncUDP udp;
    bool connected;
    unsigned long lastPacketTime;
    unsigned long lastRTPRequestTime;
    unsigned long connectionStartTime;
    tagVBAN_VMRT_PACKET currentRTPPacket;
    uint8_t commandFrameCounter;
    bool ipAddressNotSaved;

    std::vector<uint8_t> createCommandPacket(const String &command);
    std::vector<uint8_t> createRTPPacket();
    void handleUDPPacket(AsyncUDPPacket packet);
    void sendVBANCommand(const String &command);
};