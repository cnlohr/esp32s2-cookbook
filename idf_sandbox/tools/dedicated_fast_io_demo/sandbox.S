# This is a .S file.  While you can operate with volatile __asm__, it's easier many
# times to use a .S file.  Which lets you freely write assembly.

# Because this is a .S file you actually can #include things, etc.

#include "soc/gpio_reg.h"

# You can use this reference: https://0x04.net/~mwk/doc/xtensa.pdf
# A guide on how to use gas (this .S format) is here:
#    https://ftp.gnu.org/old-gnu/Manuals/gas-2.9.1/html_chapter/as_7.html
#    https://dl.espressif.com/github_assets/espressif/xtensa-isa-doc/releases/download/latest/Xtensa.pdf

.align 4

