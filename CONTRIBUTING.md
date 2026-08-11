# Contributing to fbkmshow

Thanks for considering a contribution — this is a small, focused tool, so
the bar for changes is "does it stay simple and does it work."

## Building locally

```sh
make            # native build
make all-arch   # cross-build aarch64 + armv7 + x86_64 (needs cross-toolchains, see README)
make docker     # same, via Docker, no local toolchain install needed
```

## Testing your change

There's no test suite (this is a thin wrapper around `ioctl`/`write` and
`stb_image`). Please verify manually:

```sh
make
./fbkmshow --help
./fbkmshow some-test-image.png     # against a real or virtual framebuffer device
```

If your change touches the pixel-mapping or rotation logic, test all four
`--rotate` values against a real framebuffer if you can.

## Code style

- Plain C, matching the existing minimal style in `fbkmshow.c`
- No new dependencies beyond `stb_image.h` — the point of this tool is to
  have none
- Keep it small: avoid abstractions or configuration options that aren't
  needed by a real use case

## Submitting a pull request

1. Fork the repo and create a branch for your change
2. Make sure `make all-arch` builds cleanly with no warnings
3. Open a PR describing what changed and why

## Reporting issues

Please include: the platform/SoC, the framebuffer driver in use (legacy
fbdev or DRM/KMS fbdev-emulation), the exact command you ran, and the
program's stderr output.
