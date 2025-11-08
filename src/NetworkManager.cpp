#include "NetworkManager.h"
#include <WiFi.h>
#include <Arduino.h>

// Destination IP for Voicemeeter
const IPAddress NetworkManager::DEST_IP = IPAddress(192, 168, 72, 235);

NetworkManager::NetworkManager()
    : connected(false), lastPacketTime(0), lastRTPRequestTime(0), commandFrameCounter(0), serviceFrameCounter(0) {}

bool NetworkManager::begin()
{
    // Start WiFi configuration
    Serial.println("Starting WiFiManager...");
    wifiManager.autoConnect("VOLUME");
    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());

    if (udp.listen(LOCAL_PORT))
    {
        Serial.println("UDP listening");
        // capture this pointer for callback
        udp.onPacket([this](AsyncUDPPacket packet)
                     { handleUDPPacket(packet); });
        connected = true;
        return true;
    }
    Serial.println("UDP listen failed");
    return false;
}

void NetworkManager::handleUDPPacket(AsyncUDPPacket packet)
{
    if (packet.length() < sizeof(tagVBAN_HEADER))
        return;
    tagVBAN_HEADER *header = (tagVBAN_HEADER *)packet.data();
    if (header->vban != 0x4E414256)
        return; // not VBAN
    unsigned char protocol = header->format_SR & VBAN_PROTOCOL_MASK;
    if (protocol == VBAN_PROTOCOL_SERVICE && header->format_nbc == VBAN_SERVICE_RTPACKET)
    {
        if (packet.length() >= sizeof(tagVBAN_VMRT_PACKET))
        {
            memcpy(&currentRTPPacket, packet.data(), sizeof(tagVBAN_VMRT_PACKET));
            lastPacketTime = millis();
        }
    }
}

void NetworkManager::update()
{
    // send periodic RTP request
    if (millis() - lastRTPRequestTime > 10000 || lastRTPRequestTime == 0)
    {
        std::vector<uint8_t> rtp = createRTPPacket();
        udp.writeTo(rtp.data(), rtp.size(), DEST_IP, LOCAL_PORT);
        lastRTPRequestTime = millis();
    }
}

void NetworkManager::sendCommand(const String &command)
{
    std::vector<uint8_t> packet = createCommandPacket(command);
    udp.writeTo(packet.data(), packet.size(), DEST_IP, LOCAL_PORT);
    Serial.println("sent packet: " + command);
}

void NetworkManager::incrementVolume(uint8_t channel, bool up)
{
    String command = "strip(" + String(channel + 5) + ").gain " + (up ? "+" : "-") + "= 3";
    sendCommand(command);
}

void NetworkManager::setVolume(uint8_t channel, float level)
{
    String command = "strip(" + String(channel + 5) + ").gain " + "+= " + String(level, 2);
    sendCommand(command);
}

std::vector<uint8_t> NetworkManager::createCommandPacket(const String &command)
{
    String streamName = "Command1";
    commandFrameCounter++;
    std::vector<uint8_t> pkt = {0x56, 0x42, 0x41, 0x4e, 0x40, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, commandFrameCounter, 0x00, 0x00, 0x00};
    for (int i = 0; i < 16; i++)
        pkt[i + 8] = streamName[i];
    pkt.insert(pkt.end(), command.begin(), command.end());
    return pkt;
}

std::vector<uint8_t> NetworkManager::createRTPPacket()
{
    std::vector<uint8_t> rtp_packet = {0x56, 0x42, 0x41, 0x4e, 0x60, 0x00, 0x20, 0x0f, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x20, 0x52, 0x54, 0x50, 0x01, 0x59, 0x41, 0, 0, 0, 0, 154};
    return rtp_packet;
}
