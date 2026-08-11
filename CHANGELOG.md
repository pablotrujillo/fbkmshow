# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [Unreleased]

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
