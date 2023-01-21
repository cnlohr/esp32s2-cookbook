all : hidapi_test

UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
CFLAGS:=-g -O0
LDFLAGS:=-ludev
else
CFLAGS:=-g -O0
LDFLAGS:=C:/windows/system32/setupapi.dll
endif

hidapi_test : hidapi_test.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ hidapi_test

