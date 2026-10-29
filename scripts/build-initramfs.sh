#!/bin/sh
# Build the SailfishOS bring-up initramfs: busybox (static, aarch64) + initramfs/init.
#   build-initramfs.sh /path/to/static/busybox [out.cpio.gz]
# SPLASH=image.ppm (binary PPM, 1080x2160) adds a boot splash that init draws on fb0.
# The staged tree is build/initramfs; the archive goes to build/sfos-initramfs.cpio.gz.
set -eu
bb=$1; out=${2:-build/sfos-initramfs.cpio.gz}
cd "$(dirname "$0")/.."
[ -x "$bb" ] || { echo "usage: $0 /path/busybox-static [out.cpio.gz]"; exit 1; }
rm -rf build/initramfs
mkdir -p build/initramfs/bin
install -m 755 "$bb" build/initramfs/bin/busybox
install -m 755 initramfs/init build/initramfs/init
[ -z "${SPLASH:-}" ] || install -m 644 "$SPLASH" build/initramfs/splash.ppm
(cd build/initramfs && find . | cpio -o -H newc 2>/dev/null | gzip -9) > "$out"
ls -l "$out"
