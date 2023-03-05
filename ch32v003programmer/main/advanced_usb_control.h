#ifndef _ADVANCED_USB_CONTROL_H

#include <stdint.h>
#include <stdarg.h>

/* Advanced USB Control System

    This system allows one to communicate with the ESP at runtime and issue a
    series of commands from userspace applications using HID API.  This can be
    concurrent to using the interface as any type of HID device, mouse, 
    keyboard etc.
    
    This is a convenient way of writing to flash, IRAM, etc.  And testing out
    code without needing to reflash the whole chip.
    
    The memory layout for this functionality follows the following guide.

        NOTE: the first byte is the report ID.  In some systems this is
        separate.  In others it is the first byte of the message.  You will
        need to consult examples to determine which way this is.
        
        NOTE: Windows and Linux behave differently.  In Windows, you always
        must transfer exactly REPORT_SIZE+1 bytes (With first byte being
        the report ID (0xaa).  Where Linux allows variations.
        
        NOTE: On Windows and Linux you can transfer arbitrary lengths of
        data for FEATURE_GET commands.  IF YOU KNOW HOW TO SEND ARBITRARY
        DATA LENGTHS FOR FEATURE_SET ON WINDOWS, PLEASE CONTACT ME (cnlohr)
    
    [0xaa (or report ID = 0xaa)] [1-byte COMMAND] [4-byte parameter]
        [Data or parameters ...]
        
    Note parameter are 4-bytes, LSB first byte. 

    The commands that are supported are as follows:

    AUSB_CMD_REBOOT: 0x03
        Parameter 1:
            Zero: Just reboot.
            Nonzero: Reboot into bootloader.

    AUSB_CMD_WRITE_RAM: 0x04
        Parameter 0: Address to start writing to.
        Parameter 1 [2-bytes]: Length of data to write
        Data to write...
    
    AUSB_CMD_READ_RAM: 0x05
        Parameter 0: Address to start reading from.
        (Must use "get report" to read data)
    
    AUSB_CMD_EXEC_RAM: 0x06
        Parameter 0: Address to "call"
        
    AUSB_CMD_SWITCH_MODE: 0x07
        Parameter 0: Pointer to swadege mode (will change)
            If 0, will go to main mode without reboot.
    
    AUSB_CMD_ALLOC_SCRATCH: 0x08
        Parameter 0: Size of scrach requested.
            If -1, simply report.
            If 0, deallocate scratch.
        Address and size of scratch will be available to "get reports"
        as two 4-byte parameters.

	ACMD_CMD_MEMSET: 0x09
		Parameter 0: Start of memory to memset
		Parameter 1: Byte to write.

	ACMD_CMD_GETVER: 0x0a
		Writes a 16-byte version identifier in the scratch, which can be read.
		Format TBD.

    AUSB_CMD_FLASH_ERASE: 0x10
        Parameter 0: Start address of flash to erase.
        Parameter 1 [4 bytes]: Size of flash to erase.
        
        NOTE: For flash erase commands, you MUST use sector-aligned values!
        
    AUSB_CMD_FLASH_WRITE: 0x11
        Parameter 0: Start address of flash to write.
        Parameter 1 [2 bytes]: Length of data to write
        Payload: Data to write.
        
    AUSB_CMD_FLASH_READ: 0x12
        Parameter 0: Start address of flash to read.
        Parameter 1 [2 bytes]: Quantity of flash to read (Cannot exceed
            SCRATCH_IMMEDIATE_DWORDS)
        
        The data is written to a scratch buffer
    
*/

#define SCRATCH_IMMEDIATE_DWORDS  64

#define AUSB_CMD_REBOOT           0x03
#define AUSB_CMD_WRITE_RAM        0x04
#define AUSB_CMD_READ_RAM         0x05
#define AUSB_CMD_EXEC_RAM         0x06
#define AUSB_CMD_SWITCH_MODE      0x07
#define AUSB_CMD_ALLOC_SCRATCH    0x08
#define ACMD_CMD_MEMSET           0x09
#define ACMD_CMD_GETVER           0x0a
#define AUSB_CMD_FLASH_ERASE      0x10
#define AUSB_CMD_FLASH_WRITE      0x11
#define AUSB_CMD_FLASH_READ       0x12

void advanced_usb_tick();
int handle_advanced_usb_control_get( int reqlen, uint8_t * data );
int handle_advanced_usb_terminal_get( int reqlen, uint8_t * data );
void handle_advanced_usb_control_set( int datalen, const uint8_t * data );
int advanced_usb_write_log_printf(const char *fmt, va_list args);
void advanced_usb_setup();
int uprintf( const char * fmt, ... );

struct SandboxStruct
{	
	int (*fnAdvancedUSB)( uint8_t * buffer, int reqlen, int is_get );
	void (*fnIdle)( );
	void (*fnDecom)( );
};

extern struct SandboxStruct * g_SandboxStruct;


#endif

