
UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CFLAGS:=-g -O0
LDFLAGS:=-ludev
CC:=gcc
RCMD:=./reboot_swadge_into_bootloader
else
CFLAGS:=-Os -s
CC:=gcc
LDFLAGS:=C:/windows/system32/setupapi.dll
RCMD:=./reboot_swadge_into_bootloader.exe
endif

all : $(RCMD) reboot

$(RCMD) : reboot_swadge_into_bootloader.c
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

reboot : $(RCMD)
	$(RCMD) || true

clean :
	rm -rf *.o *~ $(RCMD)
