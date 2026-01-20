# ESP32-S2 CH32V003 Programmer

Connect SWIO (Pin D1) on CH32V003 to GPIO6 on the ESP32S2.
 * Optionally use 10k pull-up on SWIO.
 * Optional Clock Synthesizer is on GPIO2 "Multi2" / "M0"
 * VCC Output on GPIO11/12 on ESP32S2.



Use a DevkitC. Once programmed, see ch32v003fun for more detailed instructions.

Tested devices are: 
* [DevkitC](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S2-DEVKITC-1-N8R2/16688755) 
* [this](https://github.com/cnlohr/cnhardware/tree/master/esp32s2-funprog)

You can use other variants of ESP32-S2, but make sure the board you are using allows you to interface USB directly. For example if it has *CP210x USB to serial converter* (like in [this version](https://www.adafruit.com/product/4693)), it might cause issues. It can be solved by wiering USB out directly.

This is the ESP32-S2 programmer for [ch32v003fun](https://github.com/cnlohr/ch32v003fun) built using [ESP-IDF](https://github.com/espressif/esp-idf.git) ~~v5.0.~~ v5.2.5

## To just flash it without building

```
esptool.py -p /dev/ttyACM1 -b 460800 --before=no_reset --after=no_reset write_flash --flash_mode dio --flash_freq 80m --flash_size 4MB 0x1000 build/bootloader/bootloader.bin 0x10000 build/usb_sandbox.bin 0x8000 build/partition_table/partition-table.bin
```
Or if you, like about half the programmers on this planet are struggling getting python to do the right thing, just use ESPUtil https://github.com/cpq/esputil

## To debug using USB

- Set ``#define DEBUG_PRINT`` to ``1`` in ``programmer_config.h``. Recompile and flash the programmer.
- Go to ``swadgeterm`` and do ``make``. Then run it, you will see messages in the terminal when the programmer is connected and running.

If you have trailing character that repeats continuously after the end of the last incoming print, change ``toprint = r - 2;`` to ``toprint = r - 3;`` in ``swadgeterm/swadgeterm.c``.
