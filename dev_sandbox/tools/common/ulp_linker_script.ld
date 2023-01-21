ENTRY(reset_vector)

MEMORY
{
    ram(RW) : ORIGIN = 0, LENGTH = 8168
}

SECTIONS
{
    . = ORIGIN(ram);
    .text :
    {
        *(.text.vectors)
        *(.text)
        *(.text*)
    } >ram

    .rodata ALIGN(4):
    {
        *(.rodata)
        *(.rodata*)
    } > ram

    .data ALIGN(4):
    {
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
    } > ram

    .bss ALIGN(4) :
    {
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
    } >ram

    __stack_top = ORIGIN(ram) + LENGTH(ram);
}

/* From esp32s2.peripherals.ld NOTE this will be different on the S3. */

PROVIDE ( RTCCNTL = 0x8000 );
PROVIDE ( RTCIO = 0xA400 );
PROVIDE ( SENS = 0xC800 );    /* ADC, DAC, Touch, Temp */
PROVIDE ( RTCI2C = 0xEC00 );
