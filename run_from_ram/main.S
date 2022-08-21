# A .s file

.global main_loop

.align 4

main_loop:
	set_bit_gpio_out 0x1
	nop.n
	nop.n
	nop.n
	nop.n
	clr_bit_gpio_out 0x1
	j main_loop
