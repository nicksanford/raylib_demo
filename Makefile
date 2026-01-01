MACOSX_DEPLOYMENT_TARGET ?= 14.0.0
CC ?= clang
PLATFORM ?= $(shell uname -s)-$(shell uname -m)
BIN_OUTPUT_PATH ?= bin/$(PLATFORM)
# RAYLIB_VERSION ?= 9b183e0c5e5786a8a92e34e6a8f941586c12c39d 
CFLAGS ?= -Wall -Wextra -Wpedantic -Wnull-dereference -Wdouble-promotion  -Wshadow -Wunused -Wenum-conversion -Wuninitialized -Werror -Wno-unused-parameter -g -fdata-sections -ffunction-sections -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize-address-use-odr-indicator -fsanitize-cfi-cross-dso -fno-common -ggdb -fsanitize=address -fsanitize-address-use-after-scope  -fsanitize-address-use-after-return=always 
BSTR_SOURCES=$(wildcard bstrlib-1.0.0/*.c)
BSTR_OBJECTS=$(patsubst %.c,%.o,$(BSTR_SOURCES))
BSTR_HEADERS=$(wildcard bstrlib-1.0.0/*.h)
BSTR_TARGET=build/lib/libbstr.a
BSTR_SO_TARGET=$(patsubst %.a,%.so,$(BSTR_TARGET))
RAYLIB_TARGET=build/lib/libraylib.a
FFMPEG_TARGET=build/lib/libffmpeg.a

SOURCE_OS ?= $(shell uname -s | tr '[:upper:]' '[:lower:]')
ifeq ($(SOURCE_OS),linux)
    NPROC ?= $(shell nproc)
else ifeq ($(SOURCE_OS),darwin)
    NPROC ?= $(shell sysctl -n hw.ncpu)
else
    NPROC ?= 1
endif

ifeq ($(PLATFORM),Darwin-arm64)
LDFLAGS += -framework CoreVideo -framework Cocoa -framework IOKit -framework GLUT -framework OpenGL
else
	echo $(PLATFORM) not yet supported. Please edit Makefile
	exit 1
endif

.PHONY: scribe clean

INCLUDES := $(wildcard build/include/*)
scribe: LDFLAGS += $(INCLUDES:%=-I%)	
scribe: build $(BSTR_TARGET) $(RAYLIB_TARGET) scribe.c 
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) $(BSTR_TARGET) $(RAYLIB_TARGET) scribe.c -o $(BIN_OUTPUT_PATH)/scribe

build:
	git submodule update --init
	@mkdir -p $(BIN_OUTPUT_PATH)
	@mkdir -p build/include 
	@mkdir -p build/lib
	@mkdir -p build/share
	@mkdir -p build/include/libbstrlib
	@mkdir -p build/include/libraylib

$(BSTR_TARGET): CFLAGS += -fPIC
$(BSTR_TARGET): build $(BSTR_OBJECTS)
	cp $(BSTR_HEADERS) ./build/include/libbstrlib/
	ar rcs $@ $(BSTR_OBJECTS)
	ranlib $@

$(BSTR_SO_TARGET): $(BSTR_TARGET) $(BSTR_OBJECTS)
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) -shared -o $@ $(BSTR_OBJECTS)

$(RAYLIB_TARGET): build 
	cp raygui.h ./build/include/libraylib/
	cp ./raylib/src/raylib.h ./build/include/libraylib/
	cd raylib/src && MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} make 
	cp ./raylib/src/libraylib.a ./build/lib

FFMPEG_CONFIG_OPTS = --prefix=../build \
		--enable-gpl \
		--disable-shared \
		--disable-programs \
		--disable-doc \
		--disable-everything \
		--enable-static \
		--enable-decoder=mpeg4 \
		--enable-decoder=h264 \
		--enable-decoder=hevc \
		--enable-decoder=mjpeg \
		--enable-muxer=mov \
		--enable-muxer=mp4 \
		--enable-demuxer=mov \
		--enable-encoder=libx264 \
		--enable-encoder=mjpeg \
		--enable-encoder=mpeg4 \
		--enable-libx264 \
		--enable-parser=h264 \
		--enable-parser=hevc \
		--enable-protocol=file

$(FFMPEG_TARGET): build
	cd FFmpeg && \
		./configure $(FFMPEG_CONFIG_OPTS) && \
		make -j$(NPROC) && \
		make install

raygui.h: 
	wget https://raw.githubusercontent.com/raysan5/raygui/refs/heads/master/src/raygui.h

raylib:

FFmpeg:
	git submodule update --init

clean:
	@rm -rf bin build $(BSTR_OBJECTS) && cd raylib/src && make clean 

