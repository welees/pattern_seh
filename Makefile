PLATFORM       := $(shell uname -s)


TYPE           := CC
LOCK_MONITOR_1 := -D LOCK_MONITOR
CONFIG_DEBUG   := -D _DEBUG
CCFLAGS        := -I. $(CONFIG_$(CONFIG)) $(LOCK_MONITOR_$(LOCK)) -Wno-unused-value
CPPFLAGS       := -I. -std=c++11 $(CONFIG_$(CONFIG)) $(LOCK_MONITOR_$(LOCK)) -Wno-unused-value

DEFAULT        := LINUX64

CC_            := gcc -D_LINUX
CCLD_          := gcc
CC_LINUX64     := gcc -D_LINUX 
CCLD_LINUX64   := gcc

CC_LINUX32     := gcc -m32 -D_LINUX 
CCLD_LINUX32   := gcc -m32 

CC_WIN32       := i686-w64-mingw32-gcc -D_MINGW -D_WIN32
CCLD_WIN32     := i686-w64-mingw32-gcc -D_MINGW -D_WIN32

CC_WIN64       := x86_64-w64-mingw32-gcc -D_MINGW -D_WIN32 
CCLD_WIN64     := x86_64-w64-mingw32-gcc 

CC_MACX64      := x86_64-apple-darwin21.4-clang -D_MACOS
CCLD_MACX64    := x86_64-apple-darwin21.4-clang 

CPP_           := g++ -D_LINUX 
CPPLD_         := g++ 
CPP_LINUX64    := g++ -D_LINUX 
CPPLD_LINUX64  := g++ 

CPP_LINUX32    := g++ -m32 -D_LINUX 
CPPLD_LINUX32  := g++ -m32 

CPP_WIN32      := i686-w64-mingw32-g++ -D_MINGW -D_WIN32
CPPLD_WIN32    := i686-w64-mingw32-g++

CPP_WIN64      := x86_64-w64-mingw32-g++ -D_MINGW -D_WIN32 
CPPLD_WIN64    := x86_64-w64-mingw32-g++ 

CPP_MACX64     := x86_64-apple-darwin21.4-clang++ -D_MACOS
CPPLD_MACX64   := x86_64-apple-darwin21.4-clang++

SUFFIX_WIN32   := .exe
SUFFIX_WIN64   := .exe

COMPILER       := $($(TYPE)_$(TARGET))
LINKER         := $($(TYPE)LD_$(TARGET))

#.PHONY:all show_usage


test_seh_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET)):SEH_NonWin.c SEH_MingW32.c test.c show_usage
	@echo TYPE=$(TYPE)
	@echo Compiler=$(LINKER)
	@echo Linker=$(LINKER)
	$(COMPILER) -O0 -g $($(TYPE)FLAGS) -c SEH_MingW32.c
	$(COMPILER) -O0 -g $($(TYPE)FLAGS) -c SEH_MingW64.c
	$(COMPILER) -O0 -g $($(TYPE)FLAGS) -c SEH_NonWin.c
	$(COMPILER) -O0 -g $($(TYPE)FLAGS) -c test.c
	$(LINKER) -Wl,-rpath,./ -pthread -o $@ SEH_MingW32.o SEH_MingW64.o SEH_NonWin.o test.o
	@echo ""
	@echo Done, test_seh_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET)) built

all: seh_test_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET))

show_usage:
	@echo "+-------------------------------------------------------------------------+"
	@echo "|                                                                         |"
	@echo "|                                                                         |"
	@echo "|                      weLees SEH virtual project                         |"
	@echo "|                                                                         |"
	@echo "|           It works for MACOSX/Linux/FreeBSD/MINGW32/MINGW64             |"
	@echo "|             please contact us with any advise or question               |"
	@echo "|                                                                         |"
	@echo "|                              weLees support group:support@welees.com    |"
	@echo "|                                                                         |"
	@echo "|                                                                         |"
	@echo "+-------------------------------------------------------------------------+"
	@echo Try make to build testee for $(TARGET) 
	@echo Try make to build testee for $(TARGET) 
	@echo You can try cmd
	@echo "    make |TARGET=...|TYPE=... to build testee for other platfrom"
	@echo "  "the available target are : "WIN32|LINUX32|LINUX64|MACX64"
	@echo "  "the available type are : "CC|CPP"
	@echo

clean:
	rm -rf *.o test_seh_$(TARGET)$(SUFFIX_$(TARGET))$(DEFAULT$(TARGET))
