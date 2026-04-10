#!/bin/bash

set -e

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
: "${MUSICD_DEVICE:=s6}"

case "$MUSICD_DEVICE" in
  s6)
    : "${TARGET_TRIPLE:=arm-unknown-linux-gnueabihf}"
    : "${MUSICD_TOOLCHAIN_DIR:=$HOME/toolchain/$TARGET_TRIPLE}"
    : "${MUSICD_TOOLCHAIN_URL:=https://github.com/SkySight-666/arm-buildroot-linux-gnueabihf/releases/download/1.0.0/arm-unknown-linux-gnueabihf.xz}"
    ;;
  a6|a6p)
    : "${TARGET_TRIPLE:=arm-buildroot-linux-uclibcgnueabihf}"
    : "${MUSICD_TOOLCHAIN_DIR:=$HOME/toolchain/armv7-eabihf--uclibc--bleeding-edge-2018.11-1}"
    : "${MUSICD_TOOLCHAIN_URL:=https://github.com/penosext/Cloudpan/releases/download/toolchains/armv7-eabihf--uclibc--bleeding-edge-2018.11-1.tar.bz2}"
    ;;
  *)
    echo "Unsupported MUSICD_DEVICE: $MUSICD_DEVICE" >&2
    exit 1
    ;;
esac

if [ -z "${CROSS_TOOLCHAIN_PREFIX:-}" ]; then
  CROSS_TOOLCHAIN_PREFIX="$MUSICD_TOOLCHAIN_DIR/bin/$TARGET_TRIPLE-"
fi

if [ ! -x "${CROSS_TOOLCHAIN_PREFIX}gcc" ]; then
  echo "Toolchain not found: ${CROSS_TOOLCHAIN_PREFIX}gcc" >&2
  echo "Download: $MUSICD_TOOLCHAIN_URL" >&2
  exit 1
fi

export CROSS_TOOLCHAIN_PREFIX

BUILD_DIR="$ROOT_DIR/build/$MUSICD_DEVICE"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "Built: $BUILD_DIR/musicd"
