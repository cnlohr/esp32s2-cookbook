#define __zero_reg__ r1

.section .vectors
.global __vectors

__vectors:
	rjmp startup
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec
	rjmp novec

startup:


	.weak	__stack
	clr	__zero_reg__
	out	0x3f, __zero_reg__ // SREG = 0

	// Set initial stack pointer.
	ldi	r29,hi8(__stack)
	ldi	r28,lo8(__stack)
	out	0x3e, r29
	out	0x3d, r28

	.global __do_copy_data
__do_copy_data:
	ldi	r17, hi8(__data_end)

	ldi	r26, lo8(__data_start)  // X register
	ldi	r27, hi8(__data_start)

	ldi	r30, lo8(__data_load_start) // Z register
	ldi	r31, hi8(__data_load_start)

_data_continue_copy:
	lpm	r0, Z+
	st	X+, r0

	cpi	r26, lo8(__data_end)
	cpc	r27, r17
	brne _data_continue_copy

_clear_bss:
	ldi r18,hi8(__bss_end)
	ldi r26,lo8(__bss_start)
	ldi r27,hi8(__bss_start)
_clear_bss_loop:
	st X+, r1
	cpi	r26,lo8(__bss_end)
	cpc	r27, r18
	brne _clear_bss_loop

	rcall main

novec:
	rjmp novec

