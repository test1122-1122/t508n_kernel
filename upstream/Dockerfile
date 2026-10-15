# Native arm64 build environment for mainline kernels (gcc from Ubuntu 26.04)
FROM ubuntu:26.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential bc bison flex libssl-dev libelf-dev lz4 cpio kmod python3 curl device-tree-compiler xz-utils ca-certificates \
 && rm -rf /var/lib/apt/lists/*
