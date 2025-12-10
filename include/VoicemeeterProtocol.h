#pragma once
#include <vector>
#include <Arduino.h>

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
