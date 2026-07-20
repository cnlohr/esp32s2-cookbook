# A .s file

.global main_loop

.align 4

main_loop:
	EE.SET_BIT_GPIO_OUT 0x1
	nop.n
	nop.n
	nop.n
	nop.n
	EE.CLR_BIT_GPIO_OUT 0x1
	j main_loop
