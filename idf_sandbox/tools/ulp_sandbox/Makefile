all : ../common/sandbox_upload run ../common/sandbox_interactive

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CFLAGS:=-g -O0
LDFLAGS:=-ludev
CC:=gcc
else
CFLAGS:=-Os -s
CC:=gcc
LDFLAGS:=C:/windows/system32/setupapi.dll
endif

SYSELF:=../../build/usb_sandbox.elf

build : 
	mkdir -p build

sandbox.o : ../common/buildhelp sandbox.c $(SYSELF) build build/ulp_program.bin
	../common/buildhelp $(SYSELF)
	xtensa-esp32s2-elf-objdump -s build/sandbox.o > build/debug_sandbox_s.txt
	xtensa-esp32s2-elf-objdump -t build/sandbox.o > build/debug_sandbox_t.txt
	xtensa-esp32s2-elf-objdump -S build/sandbox.o > build/debug_sandbox_S.txt

run : ../common/sandbox_upload sandbox.o
	../common/sandbox_upload

../common/buildhelp : ../common/buildhelp.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

../common/sandbox_upload : ../common/sandbox_upload.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

../common/sandbox_interactive : ../common/sandbox_interactive.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/ulp_program.elf : ulp_program.c
	riscv32-esp-elf-gcc \
		-I${IDF_PATH}/components/esp_common/include \
		-I${IDF_PATH}/components/ulp/ulp_riscv/ulp_core/include \
		-I${IDF_PATH}/components/soc/esp32s2/include \
		-march=rv32imc -g -Os -o $@ $^ -T ../common/ulp_linker_script.ld -nostdlib -nostartfiles
	riscv32-esp-elf-objdump -t $@ > build/ulp_program.map
	riscv32-esp-elf-objdump -S $@ > build/ulp_program.S

build/ulp_program.bin : build/ulp_program.elf
	riscv32-esp-elf-objcopy -O binary -j .text $^ $@

monitor : ../common/sandbox_interactive
	../common/sandbox_interactive

interactive : ../common/sandbox_interactive build
	../common/sandbox_interactive sandbox.c sandbox.S $(SYSELF) ulp_program.c

clean :
	rm -rf *.o *~ build/sandbox_inst.bin build/sandbox_data.bin build/buildhelp build/sandbox.o sandbox_upload build/sandbox.lds build/provided.lds build/sandbox_symbols.txt build/system_symbols.txt sandbox_interactive buildhelp.exe sandbox_upload.exe build/ulp_program.bin build/ulp_program.elf

