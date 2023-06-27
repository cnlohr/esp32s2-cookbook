# This is a .S file.  While you can operate with volatile __asm__, it's easier many
# times to use a .S file.  Which lets you freely write assembly.

# Because this is a .S file you actually can #include things, etc.

#include "soc/gpio_reg.h"

# You can use this reference: https://0x04.net/~mwk/doc/xtensa.pdf
# A guide on how to use gas (this .S format) is here:
#    https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_chapter/as_7.html
#    https://dl.espressif.com/github_assets/espressif/xtensa-isa-doc/releases/download/latest/Xtensa.pdf

.align 4

# A little weird, you store referneces and big constants up here, before your code.
# Then you reference it below.

_advanced_usb_printf_head:	.long advanced_usb_printf_head
_GPIO_IN_REG:				.long GPIO_IN_REG

#  uint32_t test_function( uint32_t parameter )
#   a2 is the first parameter, then a3, etc.
#   a2 is also the return value.
#   See this for more ABI info: http://wiki.linux-xtensa.org/index.php/ABI_Interface
#
#  This function will take on one parameter, shift it up by 16, then or it with the
#  value read from advanced_usb_printf_head
#
.align 4
.global test_function
test_function:
	entry sp, 32       # This saves off some of the registers we're using
	l32r a3, _advanced_usb_printf_head
	slli a2, a2, 16    # Putting this code here hides the latency from the l32r
	l32i a3, a3, 0
	or a2, a2, a3
	retw              # This restores the registers.

.align 4
.global minimal_function
minimal_function:
	entry	sp, 16
	retw


.align 4
.global asm_read_gpio
asm_read_gpio:
	entry	sp, 16

	l32r a2, _GPIO_IN_REG

	# you can actually fit 6 instructions in here, for free if they don't rely on the result.
	# Most of the time it doesn't work out that nicely, but here it happened to.
	# l32r's are really, _realy_ slow.

	l32i a2, a2, 0
	retw


