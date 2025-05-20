PLATFORM       := $(shell uname -s)

LOCK_MONITOR_1 := -D LOCK_MONITOR
CONFIG_DEBUG   := -D _DEBUG
CFLAGS         := -I. $(CONFIG_$(CONFIG)) $(LOCK_MONITOR_$(LOCK)) -Wno-unused-value

DEFAULT        := LINUX64

CC_            := gcc -D_LINUX 
LD_            := gcc -D_LINUX 
CC_LINUX64     := gcc -D_LINUX 
LD_LINUX64     := gcc -D_LINUX 

CC_LINUX32     := gcc -m32 -D_LINUX 
LD_LINUX32     := gcc -m32 -D_LINUX 

CC_WIN32       := i686-w64-mingw32-gcc -D_MINGW -D_WIN32
LD_WIN32       := i686-w64-mingw32-gcc -D_MINGW -D_WIN32

CC_WIN64       := x86_64-w64-mingw32-gcc -D_MINGW -D_WIN32 
LD_WIN64       := x86_64-w64-mingw32-gcc -D_MINGW -D_WIN32 

CC_MACX64      := x86_64-apple-darwin21.4-clang++ -D_MACOS
LD_MACX64      := x86_64-apple-darwin21.4-clang++ -D_MACOS

SUFFIX_WIN32   := .exe

CC             := $(CC_$(TARGET))
LD             := $(LD_$(TARGET))

#.PHONY:all show_usage


test_seh_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET)):SEH_NonWin.c SEH_MingW32.c test.c show_usage
	$(CC) -O0 -g $(CFLAGS) -c SEH_MingW32.c
	$(CC) -O0 -g $(CFLAGS) -c SEH_NonWin.c
	$(CC) -O0 -g $(CFLAGS) -c test.c
	$(LD) -Wl,-rpath,./ -pthread -o $@ SEH_MingW32.o SEH_NonWin.o test.o
	@echo Done, test_seh_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET)) built

all: seh_test_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET))

show_usage:
	@echo Try make -f Test.mak to build testee for $(TARGET) 
	@echo You can try cmd
	@echo "    "make -f Test.mak TARGET=... to build testee for other platfrom
	@echo "  "the available target are : "WIN32|LINUX32|LINUX64|MACX64"
	@echo

clean:
	rm -rf *.o test_seh_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET))
