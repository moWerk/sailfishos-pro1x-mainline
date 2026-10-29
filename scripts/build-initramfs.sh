#!/bin/sh
# Build the SailfishOS bring-up initramfs: busybox (static, aarch64) + initramfs/init.
#   build-initramfs.sh /path/to/static/busybox [out.cpio.gz]
# SPLASH=image (1080x2160; PNG, BMP or PPM) is the boot splash init draws on fb0; default
# initramfs/splash/horizon-sailfish-portrait-memeline-73.png, SPLASH= for none. Needs ImageMagick (magick).
# The staged tree is build/initramfs; the archive goes to build/sfos-initramfs.cpio.gz.
set -eu
bb=$1; out=${2:-build/sfos-initramfs.cpio.gz}
cd "$(dirname "$0")/.."
[ -x "$bb" ] || { echo "usage: $0 /path/busybox-static [out.cpio.gz]"; exit 1; }
rm -rf build/initramfs
mkdir -p build/initramfs/bin
install -m 755 "$bb" build/initramfs/bin/busybox
install -m 755 initramfs/init build/initramfs/init
SPLASH=${SPLASH-initramfs/splash/horizon-sailfish-portrait-memeline-73.png}
[ -z "$SPLASH" ] || magick "$SPLASH" -background black -alpha remove -depth 8 ppm:build/initramfs/splash.ppm
(cd build/initramfs && find . | cpio -o -H newc 2>/dev/null | gzip -9) > "$out"
ls -l "$out"
