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
FFMPEG_TARGET=build/lib/libavcodec.a
FFMPEG_LIBS=    libavformat \
                libavcodec  \
                libavutil   \
                libswscale  


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

	
main: LDFLAGS += -Ibuild/include -Lbuild/lib
main: build $(BSTR_TARGET) $(RAYLIB_TARGET) $(FFMPEG_TARGET) main.c 
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) \
		$(shell pkg-config --with-path=./build/lib/pkgconfig --libs-only-other $(FFMPEG_LIBS)) \
		$(shell pkg-config --with-path=./build/lib/pkgconfig --libs-only-l $(FFMPEG_LIBS)) \
		main.c \
		-o $(BIN_OUTPUT_PATH)/main

scribe: LDFLAGS += -Ibuild/include
scribe: build $(BSTR_TARGET) $(RAYLIB_TARGET) $(FFMPEG_TARGET) scribe.c 
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) $(wildcard build/lib/*.a) scribe.c -o $(BIN_OUTPUT_PATH)/scribe

build:
	git submodule update --init
	mkdir -p $(BIN_OUTPUT_PATH)
	mkdir -p build/include 
	mkdir -p build/lib
	mkdir -p build/share

$(BSTR_TARGET): CFLAGS += -fPIC
$(BSTR_TARGET): build $(BSTR_OBJECTS)
	cp $(BSTR_HEADERS) ./build/include
	ar rcs $@ $(BSTR_OBJECTS)
	ranlib $@

$(BSTR_SO_TARGET): $(BSTR_TARGET) $(BSTR_OBJECTS)
	$(CC) -std=c23 -O1 $(CFLAGS) $(LDFLAGS) -shared -o $@ $(BSTR_OBJECTS)

$(RAYLIB_TARGET): build 
	cp raygui.h ./raylib/src/raylib.h ./raylib/src/rlgl.h ./raylib/src/raymath.h ./build/include
	cd raylib/src && MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET} RAYLIB_RELEASE_PATH=$(abspath build/lib) make -j$(NPROC) 


FFMPEG_CONFIG_OPTS = --prefix=../build \
		--enable-gpl \
		--disable-shared \
		--disable-programs \
		--disable-doc \
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
	rm -rf bin build $(BSTR_OBJECTS) 
	cd raylib/src && make clean 
	cd FFmpeg && make clean

