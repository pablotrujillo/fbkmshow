# fbkmshow

[![CI](https://github.com/pablotrujillo/fbkmshow/actions/workflows/ci.yml/badge.svg)](https://github.com/pablotrujillo/fbkmshow/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/pablotrujillo/fbkmshow)](https://github.com/pablotrujillo/fbkmshow/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A minimal, zero-dependency command-line tool that decodes an image and writes
it directly to a Linux framebuffer device — no X11, no Wayland, no
compositor, no `libdrm`.

It works against any `/dev/fbN` node: classic legacy fbdev drivers, and the
generic fbdev-emulation layer exposed by modern DRM/KMS drivers (common on
newer SoCs, e.g. Rockchip RK3576). Some of those DRM fbdev-emulation shims
don't implement `mmap`, so `fbkmshow` only ever uses plain `write()` — the
same approach `dd` uses to push raw pixels to `/dev/fb0` — which makes it
work everywhere a framebuffer device exists at all.

## Features

- Decodes JPEG, PNG, BMP, GIF, and other formats supported by
  [stb_image](https://github.com/nothings/stb) (public domain / MIT)
- Queries the framebuffer's real geometry live via `FBIOGET_VSCREENINFO` —
  no hardcoded resolution
- Scales and centers the image to fit, preserving aspect ratio
- Optional rotation: `0`, `90`, `180`, or `270` degrees, for panels mounted
  sideways or upside down
- Converts to the framebuffer's native 32bpp pixel layout automatically
- Writes via `write()` only — works even when the driver doesn't support
  `mmap`
- Single C file, single vendored header, no shared-library dependencies —
  a static build runs on a bare embedded Linux image with nothing else
  installed

## Requirements

- A Linux system exposing a 32bpp framebuffer device (e.g. `/dev/fb0`)
- A C compiler to build from source (see below); prebuilt static binaries
  are also published on the [Releases](https://github.com/pablotrujillo/fbkmshow/releases)
  page for `aarch64`, `armv7` (armhf), and `x86_64` — just download and run

## Building

### Native (host architecture)

```sh
make
./fbkmshow --help
```

### Cross-compiling for a specific architecture

Install the matching cross-toolchain package, then run the matching target:

```sh
# Debian/Ubuntu
sudo apt-get install gcc-aarch64-linux-gnu libc6-dev-arm64-cross \
                      gcc-arm-linux-gnueabihf libc6-dev-armhf-cross gcc

make aarch64   # -> fbkmshow-aarch64  (static)
make armv7     # -> fbkmshow-armv7    (static)
make x86_64    # -> fbkmshow-x86_64   (static)
make all-arch  # builds all three
```

### Via Docker (no local toolchain install needed)

```sh
make docker
```

This builds a throwaway image from the public `ubuntu:24.04` Docker Hub
base (no private registry, no login) and cross-compiles all three
architectures inside it.

## Usage

```
fbkmshow [-h|--help] [--rotate=0|90|180|270] [--fb=/dev/fb0] <image-file>
```

| Flag              | Description                                      | Default     |
|-------------------|---------------------------------------------------|-------------|
| `--rotate=N`      | Rotate the image `0`, `90`, `180`, or `270` degrees | `0`         |
| `--fb=PATH`       | Path to the framebuffer device                     | `/dev/fb0`  |
| `-h`, `--help`    | Print usage and exit                               | —           |

### Examples

```sh
# Show an image on the default framebuffer
./fbkmshow photo.jpg

# Show it rotated 180° (e.g. panel mounted upside down)
./fbkmshow --rotate=180 photo.jpg

# Target a specific framebuffer device
./fbkmshow --fb=/dev/fb1 splash.png
```

## How it works

1. Opens the framebuffer device and queries its resolution, bit depth, and
   stride via `ioctl(FBIOGET_VSCREENINFO / FBIOGET_FSCREENINFO)`.
2. Decodes the input image with `stb_image` into RGBA.
3. Scales the image to fit the framebuffer (or the rotated logical canvas,
   for 90°/270°), centering it on a black background.
4. Converts each pixel to the framebuffer's native format while placing it
   at its rotated destination coordinates.
5. Writes the resulting buffer to the device with a single `write()` call.

Only 32bpp framebuffers are currently supported.

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## Author

[Pablo Trujillo](https://github.com/pablotrujillo)

## License

MIT — see [LICENSE](LICENSE). Bundles
[stb_image.h](https://github.com/nothings/stb) by Sean Barrett, dual
licensed public domain / MIT.
