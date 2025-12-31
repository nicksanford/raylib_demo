MACOSX_DEPLOYMENT_TARGET ?= 14.0.0
CC ?= clang
PLATFORM ?= $(shell uname -s)-$(shell uname -m)
BIN_OUTPUT_PATH ?= bin/$(PLATFORM)
# RAYLIB_VERSION ?= 9b183e0c5e5786a8a92e34e6a8f941586c12c39d 
CFLAGS ?= -Wall -Wextra -Wpedantic -Wnull-dereference -Wdouble-promotion  -Wshadow -Wunused -Wenum-conversion -Wuninitialized -Werror -Wno-unused-parameter -g -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize-address-use-odr-indicator -fsanitize-cfi-cross-dso -fno-common -ggdb -fsanitize=address -fsanitize-address-use-after-scope  -fsanitize-address-use-after-return=always 
BSTR_SOURCES=$(wildcard bstrlib-1.0.0/*.c)
BSTR_OBJECTS=$(patsubst %.c,%.o,$(BSTR_SOURCES))
BSTR_TARGET=build/libbstr.a
BSTR_SO_TARGET=$(patsubst %.a,%.so,$(BSTR_TARGET))
RAYLIB_TARGET=build/libraylib.a

ifeq ($(PLATFORM),Darwin-arm64)
LDFLAGS += -framework CoreVideo -framework Cocoa -framework IOKit -framework GLUT -framework OpenGL
else
	echo $(PLATFORM) not yet supported. Please edit Makefile
	exit 1
endif

.PHONY: scribe clean

scribe: LDFLAGS += -I./raylib/src -I./bstrlib-1.0.0 
scribe: build $(BSTR_TARGET) $(RAYLIB_TARGET) raygui.h scribe.c 
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) $(BSTR_TARGET) $(RAYLIB_TARGET) scribe.c -o $(BIN_OUTPUT_PATH)/scribe

build:
	@mkdir -p $(BIN_OUTPUT_PATH)
	@mkdir -p build

$(BSTR_TARGET): CFLAGS += -fPIC
$(BSTR_TARGET): build $(BSTR_OBJECTS)
	ar rcs $@ $(BSTR_OBJECTS)
	ranlib $@

$(BSTR_SO_TARGET): $(BSTR_TARGET) $(BSTR_OBJECTS)
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) -shared -o $@ $(BSTR_OBJECTS)

$(RAYLIB_TARGET): build raylib
	cd raylib/src && MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} make 
	cp ./raylib/src/libraylib.a ./build/

raygui.h: 
	wget https://raw.githubusercontent.com/raysan5/raygui/refs/heads/master/src/raygui.h

raylib:
	git clone https://github.com/raysan5/raylib.git --depth 1

clean:
	@rm -rf bin build $(BSTR_OBJECTS) && cd raylib/src && make clean 

