# ESP32-Voicemeeter-Monitoring-and-Control

ESP32 Voicemeeter Monitoring and Control, using VBAN for control (easy) and VBAN RT PACKET UDP service for monitoring (less easy)

## Hardware

Using an ESP32 WROVER Module, with a round touch lcd (https://www.aliexpress.com/item/1005004911604497.html?spm=a2g0o.order_list.order_list_main.11.1f081802s39Mqr)
Pins are defined in the platformio.ini file, and can be matched to the pinout of the board.

## Software

Still a work in progress, currently can monitor bus 1, 2 and 3, and control the volume of these buses.
Will add bus output control in the future.
