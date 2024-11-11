# ESP32-Voicemeeter-Monitoring-and-Control

ESP32 Voicemeeter Monitoring and Control, using VBAN for control (easy) and VBAN RT PACKET UDP service for monitoring (less easy).

Very much a work in progress.

## Hardware

Using an ESP32 WROVER Module, with a round touch lcd (https://www.aliexpress.com/item/1005004911604497.html?spm=a2g0o.order_list.order_list_main.11.1f081802s39Mqr)
Pins are defined in the platformio.ini file, and can be matched to the pinout of the board.

## Software

Still a work in progress, currently can monitor bus 1, 2 and 3, and control the volume of these buses.
Will add bus output control in the future.

Information about the protocol was mostly obtained by reverse engineering the VM Streamer application, and studying some PCAPs.

## Setup

Define the IP of your Voicemeeter computer in main.cpp. Turn on VBAN and enable the incoming UTF8 stream with the default name (Command1). Don't think you need the IP of the ESP in VBAN, but doesn't hurt.
