all : swadgeterm

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

swadgeterm : swadgeterm.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

monitor : swadgeterm
	./swadgeterm

clean :
	rm -rf swadgeterm