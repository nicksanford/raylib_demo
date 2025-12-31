MACOSX_DEPLOYMENT_TARGET ?= 14.0.0
CC ?= clang
PLATFORM ?= $(shell uname -s)-$(shell uname -m)
BIN_OUTPUT_PATH ?= bin/$(PLATFORM)
# RAYLIB_VERSION ?= 9b183e0c5e5786a8a92e34e6a8f941586c12c39d 
CWFLAGS ?= -Wall -Wextra -Wpedantic -Wnull-dereference -Wdouble-promotion  -Wshadow -Wunused -Wenum-conversion -Wuninitialized -Werror -Wno-unused-parameter 
aOPTIONS ?= -g -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize-address-use-odr-indicator -fsanitize-cfi-cross-dso -fno-common -ggdb -fsanitize=address -fsanitize-address-use-after-scope  -fsanitize-address-use-after-return=always 

ifeq ($(PLATFORM),Darwin-arm64)
LDFLAGS += -framework CoreVideo -framework Cocoa -framework IOKit -framework GLUT -framework OpenGL
else
	echo $(PLATFORM) not yet supported. Please edit Makefile
	exit 1
endif

.PHONY: scribe clean

scribe: scribe.c raylib/src/libraylib.a raygui.h
	mkdir -p $(BIN_OUTPUT_PATH)
	$(CC) -std=c23 -O1 $(CWFLAGS) $(COPTIONS) -I./raylib/src $(LDFLAGS) ./raylib/src/libraylib.a scribe.c -o $(BIN_OUTPUT_PATH)/scribe

raylib/src/libraylib.a: raylib
	cd raylib/src && MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} make

raygui.h: 
	wget https://raw.githubusercontent.com/raysan5/raygui/refs/heads/master/src/raygui.h

raylib:
	git clone https://github.com/raysan5/raylib.git --depth 1

clean:
	rm -rf bin && cd raylib/src && make clean
