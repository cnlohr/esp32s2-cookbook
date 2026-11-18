# ESP32-S2 CH32V003 Programmer

Connect SWIO (Pin D1) on CH32V003 to GPIO6 on the ESP32S2.
 * Optionally use 10k pull-up on SWIO.
 * Optional Clock Synthesizer is on GPIO2 "Multi2" / "M0"
 * VCC Output on GPIO11/12 on ESP32S2.



Use a DevkitC. Once programmed, see ch32fun for more detailed instructions.

Tested devices are: 
* [DevkitC](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S2-DEVKITC-1-N8R2/16688755) 
* [this](https://github.com/cnlohr/cnhardware/tree/master/esp32s2-funprog)

You can use other variants of ESP32-S2, but make sure the board you are using allows you to interface USB directly. For example if it has *CP210x USB to serial converter* (like in [this version](https://www.adafruit.com/product/4693)), it might cause issues. It can be solved by wiering USB out directly.

This is the ESP32-S2 programmer for [ch32fun](https://github.com/cnlohr/ch32fun) built using [ESP-IDF](https://github.com/espressif/esp-idf.git) ~~v5.0.~~ v5.2.5. It now supports all common RiscV chips made by WCH.

## To build

Edit config to set options like ``HW_VERSION``, ``ENABLE_RGB`` and ``USE_CDC_UART``. Hardware of the first revision has populated 6-pin leds, revision 2 uses smaller 4-pin footprint. ``USE_CDC_UART`` will enable UART on M0(DTR), M1(TX) and M2(RX) pins. ``DEBUG_PRINT`` will enable printing to [swagterm](./swadgeterm).

From inside this directory do:

``source /path/to/espidf_5.2.5/esp-idf/export.sh``

``idf.py``

Press and hold BOOT button on your programmer while plugging in USB cable, quickly do:

``idf.flash``

Disconnect and reconnect the programmer, use it with minichlink.

Or try this:

## To just flash it without building

```
esptool.py -p /dev/ttyACM0 -b 460800 --before=no-reset --after=no-reset write_flash --flash-mode dio --flash_freq 80m --flash_size 4MB 0x1000 build/bootloader/bootloader.bin 0x10000 build/funprog_esp32s2_(version).bin 0x8000 build/partition_table/partition-table.bin
```
Or if you, like about half the programmers on this planet are struggling getting python to do the right thing, just use ESPUtil https://github.com/cpq/esputil

## To debug using USB

- Set ``#define DEBUG_PRINT`` to ``1`` in ``programmer_config.h``. Recompile and flash the programmer.
- Go to ``swadgeterm`` and do ``make``. Then run it, you will see messages in the terminal when the programmer is connected and running.

If you have trailing character that repeats continuously after the end of the last incoming print, change ``toprint = r - 2;`` to ``toprint = r - 3;`` in ``swadgeterm/swadgeterm.c``.

## Controlling RGB leds and UART

You can send commands via ``minichlink`` to control extended functions of this programmer:

- ``minichlink -X u0`` - Turn UART ON(1) or OFF(0)
- ``minichlink -X U1`` - same but also save the setting to flash
- ``minichlink -X a0`` - Turn RGB animation ON(1) or OFF(0)
- ``minichlink -X A1`` - same but also save the setting to flash
- ``minichlink -X b128`` - Set maximum brightness of the LEDs (1-255, 0 sets it to maximum)
- ``minichlink -X B0`` - same but makes setting persistent
- ``minichlink -X 128:128:128`` - Set all LEDs to R:G:B color (0-255). To set each LED separately send 9 values
