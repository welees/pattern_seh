PLATFORM       := $(shell uname -s)
LOCK_MONITOR_1 := -D LOCK_MONITOR
CONFIG_DEBUG   := -D _DEBUG
CFLAGS         := $(PLATFORM) $(CONFIG_$(CONFIG)) $(LOCK_MONITOR_$(LOCK)) -Wno-unused-value
CC             := gcc
LD             := gcc

sehlinux_test:SEH.o test.o
	$(LD) -Wl,-rpath,./ -pthread -o $@ SEH.o test.o

SEH.o:SEH.c
	$(CC) -I./ -O0 -g -D $(CFLAGS) -c $<

test.o:test.c
	$(CC) -I./ -O0 -g -D $(CFLAGS) -c $<
	
all:sehlinux_test

clean:
	rm -rf *.o sehlinux_test
