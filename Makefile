# Makefile — fbkmshow: minimal multiarch Linux framebuffer image viewer.
#
# Targets:
#   make            — build natively for the host (dynamic linking)
#   make aarch64    — cross-compile a static aarch64 binary  (fbkmshow-aarch64)
#   make armv7      — cross-compile a static armv7/armhf binary (fbkmshow-armv7)
#   make x86_64     — build a static x86_64 binary (fbkmshow-x86_64)
#   make all-arch   — build all three of the above
#   make docker     — build all three inside a public ubuntu:24.04 container,
#                      no local cross-toolchain install needed
#   make clean      — remove all built binaries
#
# Cross-toolchain packages (Debian/Ubuntu):
#   sudo apt-get install gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf gcc

CC     ?= cc
STRIP  ?= strip
CFLAGS ?= -O2 -Wall -Wextra
LDLIBS := -lm
TARGET ?= fbkmshow

.PHONY: all aarch64 armv7 x86_64 all-arch build-one docker clean

all: $(TARGET)

$(TARGET): fbkmshow.c stb_image.h
	$(CC) $(CFLAGS) -o $(TARGET) fbkmshow.c $(LDLIBS)

aarch64:
	$(MAKE) build-one TARGET=fbkmshow-aarch64 CC=aarch64-linux-gnu-gcc STRIP=aarch64-linux-gnu-strip CFLAGS="-O2 -Wall -Wextra -static"

armv7:
	$(MAKE) build-one TARGET=fbkmshow-armv7 CC=arm-linux-gnueabihf-gcc STRIP=arm-linux-gnueabihf-strip CFLAGS="-O2 -Wall -Wextra -static"

x86_64:
	$(MAKE) build-one TARGET=fbkmshow-x86_64 CC=gcc STRIP=strip CFLAGS="-O2 -Wall -Wextra -static"

build-one: $(TARGET)
	$(STRIP) $(TARGET)

all-arch: aarch64 armv7 x86_64

docker:
	docker build -t fbkmshow-builder .
	docker run --rm -v "$(CURDIR):/work" -w /work fbkmshow-builder make all-arch

clean:
	rm -f fbkmshow fbkmshow-aarch64 fbkmshow-armv7 fbkmshow-x86_64
