#include "NetworkingManager.h"

NetworkingManager::NetworkingManager() : connected(false), lastPacketTime(0), lastRTPRequestTime(0), commandFrameCounter(0)
{
    ipAddressNotSaved = false;
}

void NetworkingManager::setupStores()
{
    preferences.begin("ipLastDigits", false);
}

bool NetworkingManager::begin()
{
    wifiManager.autoConnect("VOLUME");

    Serial.print("Connected! IP address: ");
    Serial.println(WiFi.localIP());

    DEST_IP = IPAddress(WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], preferences.getChar("ipLastDigits", 2)); // Voicemeeter Potato IP
    Serial.print("Destination IP set to: ");
    Serial.println(DEST_IP);

    if (udp.listen(LOCAL_PORT))
    {
        Serial.println("UDP connected");
        udp.onPacket([this](AsyncUDPPacket packet)
                     { handleUDPPacket(packet); });
        return true;
    }
    else
    {
        Serial.println("UDP connection failed");
        return false;
    }
}

void NetworkingManager::update()
{
    if (millis() - lastRTPRequestTime > 10000 || lastRTPRequestTime == 0)
    {
        // need to keep sending requests for more realtime data
        std::vector<uint8_t> rtp_packet = createRTPPacket();
        udp.writeTo(rtp_packet.data(), rtp_packet.size(), DEST_IP, LOCAL_PORT);
        lastRTPRequestTime = millis();
    }
    if (lastPacketTime == 0 || (millis() - lastPacketTime > 5000) || (WiFi.status() != WL_CONNECTED))
    {
        // haven't received data for a while, consider ourselves disconnected
        connected = false;
        connectionStartTime = 0;
    }
    else
    {
        connected = true;
        if (connectionStartTime == 0)
            connectionStartTime = millis();
    }
}

void NetworkingManager::handleUDPPacket(AsyncUDPPacket packet)
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
            if (ipAddressNotSaved)
            {
                preferences.putChar("ipLastDigits", DEST_IP[3]);
                ipAddressNotSaved = false;
            }
        }
    }
}

void NetworkingManager::sendCommand(const NetworkCommand &command)
{
    switch (command.type)
    {
    case NetworkCommandType::SEND_VBAN_COMMAND:
    {
        std::vector<uint8_t> commandPacket = createCommandPacket(command.payload);
        udp.writeTo(commandPacket.data(), commandPacket.size(), DEST_IP, LOCAL_PORT);
        Serial.println("sent packet");

        break;
    }
    case NetworkCommandType::SET_IP:
    {
        Serial.print("Setting new IP ending to: ");
        Serial.println(command.payload);
        byte lastDigits = atoi(&command.payload[strlen(command.payload) - 3]);
        String firstDigitsOfCurrentIP = String(WiFi.localIP()[0]) + "." + String(WiFi.localIP()[1]) + "." + String(WiFi.localIP()[2]) + ".";
        String newIPString = firstDigitsOfCurrentIP + String(lastDigits);
        auto newIP = IPAddress();
        newIP.fromString(newIPString);
        DEST_IP = newIP;

        ipAddressNotSaved = true; // only save when we get a response back
        std::vector<uint8_t> rtp_packet = createRTPPacket();
        udp.writeTo(rtp_packet.data(), rtp_packet.size(), DEST_IP, LOCAL_PORT);
        break;
    }
    default:
        break;
    }
}

void NetworkingManager::sendVBANCommand(const String &command)
{
    NetworkCommand item;
    item.type = NetworkCommandType::SEND_VBAN_COMMAND;
    strncpy(item.payload, command.c_str(), sizeof(item.payload));
    item.payload[sizeof(item.payload) - 1] = '\0';
    sendCommand(item);
}

void NetworkingManager::incrementVolume(uint8_t channel, bool up)
{
    String command = "strip(" + String(channel + 5) + ").gain " + (up ? "+" : "-") + "= 3";
    sendVBANCommand(command);
}
void NetworkingManager::incrementVolume(uint8_t channel, float level)
{
    String command = "strip(" + String(channel + 5) + ").gain " + "+= " + String(level, 2);
    sendVBANCommand(command);
}

std::vector<uint8_t> NetworkingManager::createCommandPacket(const String &command)
{
    String streamName = "Command1";
    commandFrameCounter++;
    std::vector<uint8_t> packet = {0x56, 0x42, 0x41, 0x4e, 0x40, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, commandFrameCounter, 0x00, 0x00, 0x00};
    for (int i = 0; i < 16; i++)
    {
        packet[i + 8] = streamName[i];
    }
    packet.insert(packet.end(), command.begin(), command.end());

    // Serial.print("Mute packet: ");
    // printVector(mute_packet);
    // Serial.print("Size of mute_packet: ");
    // Serial.println(sizeof(mute_packet));
    return packet;
}

std::vector<uint8_t> NetworkingManager::createRTPPacket()
{
    std::vector<uint8_t> rtp_packet = {0x56, 0x42, 0x41, 0x4e, 0x60, 0x00, 0x20, 0x0f, 0x52, 0x65, 0x67, 0x69, 0x73, 0x74, 0x65, 0x72, 0x20, 0x52, 0x54, 0x50, 0x01, 0x59, 0x41, 0, 0, 0, 0, 154};
    return rtp_packet;
}

char NetworkingManager::getDestIP()
{
    return preferences.getChar("ipLastDigits", 2);
}

uint32_t NetworkingManager::getDeviceIP()
{
    return WiFi.localIP();
}
// void printVector(std::vector<uint8_t> vec)
// {
//     for (int i = 0; i < vec.size(); i++)
//     {
//         Serial.print(vec[i], HEX);
//         Serial.print(" ");
//     }
//     Serial.println();
// }

// sendCommand("strip(1).mute = 1");
// sendCommand("Strip[5].A1 = 1");
// sendCommand("System.KeyPress(\"MEDIAPAUSE / MEDIAPLAY\")");