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
- Plays animated GIFs frame-by-frame, honoring each frame's own delay from
  the file, with an optional loop count (`--loops`)
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

## Installing on Debian/Ubuntu

Each [Release](https://github.com/pablotrujillo/fbkmshow/releases) also
includes a `.deb` package per architecture:

```sh
# arm64, armhf, or amd64 — pick the one matching your system
sudo apt install ./fbkmshow_<version>_arm64.deb
```

This installs `fbkmshow` to `/usr/bin/fbkmshow`. Uninstall with
`sudo apt remove fbkmshow`.

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
fbkmshow [-h|--help] [--rotate=0|90|180|270] [--fb=/dev/fb0] [--loops=N] <image-file>
```

| Flag              | Description                                      | Default     |
|-------------------|---------------------------------------------------|-------------|
| `--rotate=N`      | Rotate the image `0`, `90`, `180`, or `270` degrees | `0`         |
| `--fb=PATH`       | Path to the framebuffer device                     | `/dev/fb0`  |
| `--loops=N`       | Animated GIFs only: play the animation `N` times (`0` = forever) | `1` |
| `-h`, `--help`    | Print usage and exit                               | —           |

### Examples

```sh
# Show an image on the default framebuffer
./fbkmshow photo.jpg

# Show it rotated 180° (e.g. panel mounted upside down)
./fbkmshow --rotate=180 photo.jpg

# Target a specific framebuffer device
./fbkmshow --fb=/dev/fb1 splash.png

# Play an animated GIF twice, then exit on its last frame
./fbkmshow --loops=2 spinner.gif

# Play an animated GIF forever (e.g. a boot/loading screen)
./fbkmshow --loops=0 loading.gif
```

## How it works

1. Opens the framebuffer device and queries its resolution, bit depth, and
   stride via `ioctl(FBIOGET_VSCREENINFO / FBIOGET_FSCREENINFO)`.
2. Decodes the input image with `stb_image` into RGBA. Animated GIFs are
   decoded through `stb_image`'s dedicated multi-frame GIF path
   (`stbi_load_gif_from_memory`), which also returns each frame's delay;
   every other format (and single-frame GIFs) goes through the plain
   single-image decoder.
3. For each frame: scales it to fit the framebuffer (or the rotated logical
   canvas, for 90°/270°), centering it on a black background.
4. Converts each pixel to the framebuffer's native format while placing it
   at its rotated destination coordinates.
5. Writes the resulting buffer to the device with a single `write()` call.
   For animated GIFs, sleeps for that frame's delay, then repeats for the
   next frame — looping `--loops` times (or forever if `0`).

Only 32bpp framebuffers are currently supported.

## Project layout

```
src/           this project's source (fbkmshow.c)
third_party/   vendored dependencies (stb_image.h)
```

## Contributing

Contributions are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md).

## Author

[Pablo Trujillo](https://github.com/pablotrujillo)

## License

MIT — see [LICENSE](LICENSE). Bundles
[stb_image.h](https://github.com/nothings/stb) by Sean Barrett, dual
licensed public domain / MIT.
