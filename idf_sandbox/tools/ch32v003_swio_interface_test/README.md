# ESP32-S2 Demo Communicating with a CH32V003's debug interface.

This is me testing with a CH32V003 and an ESP32S2 over the CH32V003's debug pin.

It totally wakes up and you can read/write to the debug registers, but I haven't written code to allow you to flash it.

It's configured so:

ESP32-S2 Pin 5: Pull-Up
ESP32-S2 Pin 6: SWIO

PLACE 10k RESISTOR BETWEEN SWIO and Pull-Up.


