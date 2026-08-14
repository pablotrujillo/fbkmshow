# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [1.1.1] - 2026-08-13

### Fixed

- Scale-to-fit now uses bilinear interpolation instead of nearest-neighbor
  sampling, so downscaled images (especially text/fine detail) render
  noticeably smoother.

## [1.1.0] - 2026-08-13

### Added

- Animated GIF playback: multi-frame GIFs now play back frame-by-frame via
  `stb_image`'s `stbi_load_gif_from_memory`, honoring each frame's own delay.
- `--loops=N` flag to control how many times an animated GIF plays (`0` =
  forever). Ignored for single-frame images, which behave exactly as before.
- `SIGTERM` now stops a running animation cleanly at the next frame boundary
  instead of relying on the OS's default abrupt termination — useful for a
  `--loops=0` animation meant to run until another process signals it to
  stop.
- `--version` flag, backed by a `FBKMSHOW_VERSION` constant in the source —
  the binary's version no longer depends solely on the git tag it happened
  to be built from.
- Git checkouts now override `FBKMSHOW_VERSION` at build time with
  `git describe --tags --always --dirty`, so a non-exact-tag build (extra
  commits, `-dirty`) is visible at a glance in `--version` output; tarball
  builds with no `.git` fall back to the hardcoded constant.

## [1.0.0] - 2026-08-11

### Added

- Initial public release.
- Decode JPEG/PNG/BMP/GIF/etc via `stb_image` and write directly to a
  Linux framebuffer device (`/dev/fbN`), supporting both legacy fbdev
  drivers and DRM/KMS fbdev-emulation shims.
- Live framebuffer geometry detection via `FBIOGET_VSCREENINFO`.
- Scale-to-fit with centering, and optional `--rotate=0|90|180|270`.
- `-h`/`--help` flag.
- Multiarch build support: native, `aarch64`, `armv7`, `x86_64`, and a
  Docker-based build using the public `ubuntu:24.04` image.
- GitHub Actions CI (build check) and release workflow (publishes prebuilt
  binaries for all three architectures on tagged releases).
- `.deb` packages (`arm64`, `armhf`, `amd64`) published alongside the raw
  binaries on each release.
