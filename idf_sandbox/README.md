# ESP32-S2 hot-code-reloading example via USB port only.

## Using stubs to change to/from bootloader

This tool is used to show off a few things.
1) Use of build steps to `CMakeLists.txt` both "reboot-into-bootloader"
2) Use of build steps in `CMakeLists.txt` to "reboot-from-bootloader-into-application"

The above to things make it possible to do zero-press code reflashing of full ESP32-S2 projects compiled with the IDF.  Note that rebooting from bootloader doesn't need USB support, and uses the bootloader to manually launch the application.

```
idf.py -p /dev/ttyACM0 stub_bootload flash stub_run
```

## "sandbox" for hot-reloading

Use a "sandbox" program that is loaded over USB to a live operating ESP32-S2 for recompilation in about 200ms.  

From `tools/sandbox_test`

Type `make interactive`

Any time you save `sandbox.c` the program will recompile.


## Appendix Notes

The actual code for making a stub that can be run from the booloader and then cause it to force-reset the USB you must call 

```c
	// Maybe we need to consider tweaking these?
	chip_usb_set_persist_flags( 0 );  //NOT USBDC_PERSIST_ENA (1<<31)

	// We **must** unset this, otherwise we'll end up back in the bootloader.
	REG_WRITE(RTC_CNTL_OPTION1_REG, 0);
	void software_reset( uint32_t x );
	software_reset( 0 );  // This was the first one I tried.  It worked.
```


