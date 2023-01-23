// This file is designed to be executed by esptool's load_ram functionality.
// It compiles to ~32 bytes and gets uploaded and executed when compiled using
// the very unusual esp32_s2_stub.ld.

#include "soc/dport_access.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"  // for WRITE_PERI_REG

// The ROM locations for these will be defined in the 
void esp_cpu_reset(uint32_t x); 
void chip_usb_set_persist_flags( uint32_t x );

void bootload_reboot_stub()
{
	// Maybe we need to consider tweaking these?
	chip_usb_set_persist_flags( 0 );  //USBDC_PERSIST_ENA

	// We **must** unset this, otherwise we'll end up back in the bootloader.
	REG_WRITE(RTC_CNTL_OPTION1_REG, 0);
	void software_reset( uint32_t x );
	software_reset( 0 );  // This was the first one I tried.  It worked.

	// These should probably be investigated as other alternatives.
	//esp_cpu_reset( 0 );
}


