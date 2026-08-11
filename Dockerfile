# Self-contained build environment for fbkmshow's multiarch cross-compiles.
# Uses only the public Docker Hub ubuntu:24.04 image — no private registry,
# no login required.
#
# Usage: make docker   (see Makefile)

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        gcc-aarch64-linux-gnu \
        libc6-dev-arm64-cross \
        gcc-arm-linux-gnueabihf \
        libc6-dev-armhf-cross \
        make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
