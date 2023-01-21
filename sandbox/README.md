# ESP32-S2 hot-code-reloading example via USB port only.

This tool is used to show off a few things.
1) Use of build steps to `CMakeLists.txt` both "reboot-into-bootloader"
2) Use of build steps in `CMakeLists.txt` to "reboot-from-bootloader-into-application"

The above to things make it possible to do zero-press code reflashing of full ESP32-S2 projects compiled with the IDF.  Note that rebooting from bootloader doesn't need USB support, and uses the bootloader to manually launch the application.

```
idf.py -p /dev/ttyACM0 stub_bootload flash stub_run
```
3) Use a "sandbox" program that is loaded over USB to a live operating ESP32-S2 for recompilation in 
