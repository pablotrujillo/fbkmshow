# Makefile — fbkmshow: minimal multiarch Linux framebuffer image viewer.
#
# Layout:
#   src/            — this project's own source
#   third_party/    — vendored dependencies (stb_image.h)
#
# Targets:
#   make               — build natively for the host (dynamic linking)
#   make aarch64       — cross-compile a static aarch64 binary  (fbkmshow-aarch64)
#   make armv7         — cross-compile a static armv7/armhf binary (fbkmshow-armv7)
#   make x86_64        — build a static x86_64 binary (fbkmshow-x86_64)
#   make all-arch      — build all three of the above
#   make docker        — build all three inside a public ubuntu:24.04 container,
#                         no local cross-toolchain install needed
#   make clean         — remove the build directory
#
# All artifacts are written to the BUILDDIR directory (default: build/).
# Override it to build elsewhere, e.g.:
#   make BUILDDIR=dist
#   make all-arch BUILDDIR=/tmp/out
#
# Cross-toolchain packages (Debian/Ubuntu):
#   sudo apt-get install gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
#                        gcc-arm-linux-gnueabihf libc6-dev-armhf-cross gcc

BUILDDIR ?= build
CC       ?= cc
STRIP    ?= strip
CFLAGS   ?= -O2 -Wall -Wextra
LDLIBS   := -lm
TARGET   ?= fbkmshow
SRC      := src/fbkmshow.c
BIN      := $(BUILDDIR)/$(TARGET)

# Build-time version override: when built from a git checkout, embed
# `git describe` (e.g. "v1.0.0-3-gc199d94", or "-dirty" appended for
# uncommitted changes) so the binary can tell you it's NOT an exact tagged
# release — falls back to the source's own FBKMSHOW_VERSION constant when
# building from a tarball with no .git (describe unavailable).
GIT_VERSION := $(shell git describe --tags --always --dirty 2>/dev/null)
CPPFLAGS := -Ithird_party
ifneq ($(GIT_VERSION),)
CPPFLAGS += -DFBKMSHOW_GIT_VERSION=\"$(GIT_VERSION)\"
endif

.PHONY: all aarch64 armv7 x86_64 all-arch build-one docker clean

all: $(BIN)

$(BIN): $(SRC) third_party/stb_image.h | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $(BIN) $(SRC) $(LDLIBS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

aarch64:
	$(MAKE) build-one TARGET=fbkmshow-aarch64 CC=aarch64-linux-gnu-gcc STRIP=aarch64-linux-gnu-strip CFLAGS="-O2 -Wall -Wextra -static" BUILDDIR=$(BUILDDIR)

armv7:
	$(MAKE) build-one TARGET=fbkmshow-armv7 CC=arm-linux-gnueabihf-gcc STRIP=arm-linux-gnueabihf-strip CFLAGS="-O2 -Wall -Wextra -static" BUILDDIR=$(BUILDDIR)

x86_64:
	$(MAKE) build-one TARGET=fbkmshow-x86_64 CC=gcc STRIP=strip CFLAGS="-O2 -Wall -Wextra -static" BUILDDIR=$(BUILDDIR)

build-one: $(BIN)
	$(STRIP) $(BIN)

all-arch: aarch64 armv7 x86_64

docker:
	docker build -t fbkmshow-builder .
	docker run --rm -v "$(CURDIR):/work" -w /work fbkmshow-builder make all-arch BUILDDIR=$(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
