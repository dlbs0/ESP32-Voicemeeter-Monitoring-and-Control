#pragma once
#include <vector>
#include <Arduino.h>

// VOICEMEETER POTATO Protocol Definitions
struct tagVBAN_VMRT_PACKET
{
    unsigned char magic[4]; // VBAN
    unsigned char subProtocol;
    unsigned char function;
    unsigned char service;        // 32 = VBAN_SERVICE_RTPACKETREGISTER
    unsigned char additionalInfo; //
    unsigned char streamName[16]; // stream name
    unsigned long frameCounter;
    unsigned char voicemeeterType;    // 1 = Voicemeeter, 2= Voicemeeter Banana, 3 Potato
    unsigned char reserved;           // unused
    unsigned short buffersize;        // main stream buffer size
    unsigned long voicemeeterVersion; // version like for VBVMR_GetVoicemeeterVersion() functino
    unsigned long optionBits;         // unused
    unsigned long samplerate;         // main stream samplerate
    short inputLeveldB100[34];        // pre fader input peak level in dB * 100
    short outputLeveldB100[64];       // bus output peak level in dB * 100
    unsigned long TransportBit;       // Transport Status
    unsigned long stripState[8];      // Strip Buttons Status
    unsigned long busState[8];        // Bus Buttons Status
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
};

struct tagVBAN_HEADER
{
    unsigned long vban;       // contains 'V' 'B', 'A', 'N'
    unsigned char format_SR;  // SR index
    unsigned char format_nbs; // nb sample per frame (1 to 256)
    unsigned char format_nbc; // nb channel (1 to 256)
    unsigned char format_bit; // mask = 0x07
    char streamname[16];      // stream name
    unsigned long nuFrame;    // growing frame number
};

#define VBAN_PROTOCOL_MASK 0xE0
#define VBAN_PROTOCOL_SERVICE 0x60
#define VBAN_SERVICE_RTPACKETREGISTER 32
#define VBAN_SERVICE_RTPACKET 33

#define VMRTSTATE_MODE_BUSA1 0x00001000
#define VMRTSTATE_MODE_BUSA2 0x00002000
#define VMRTSTATE_MODE_BUSA3 0x00004000
#define VMRTSTATE_MODE_BUSA4 0x00008000
#define VMRTSTATE_MODE_BUSA5 0x00080000