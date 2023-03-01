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

sandbox.o : ../common/buildhelp sandbox.c $(SYSELF) build
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

monitor : ../common/sandbox_interactive
	../common/sandbox_interactive

interactive : ../common/sandbox_interactive build
	../common/sandbox_interactive sandbox.c sandbox.S $(SYSELF)

clean :
	rm -rf *.o *~ build/sandbox_inst.bin build/sandbox_data.bin build/buildhelp build/sandbox.o sandbox_upload build/sandbox.lds build/provided.lds build/sandbox_symbols.txt build/system_symbols.txt sandbox_interactive buildhelp.exe sandbox_upload.exe

