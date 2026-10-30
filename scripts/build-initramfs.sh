#!/bin/sh
# Build the SailfishOS bring-up initramfs: busybox (static, aarch64) + initramfs/init.
#   build-initramfs.sh /path/to/static/busybox [out.cpio.gz]
# SPLASH=image (1080x2160; PNG, BMP or PPM) is the boot splash init draws on fb0; default
# initramfs/splash/horizon-sailfish-portrait-memeline-73.png, SPLASH= for none. Needs ImageMagick (magick).
# The staged tree is build/initramfs; the archive goes to build/sfos-initramfs.cpio.gz.
# INIT=initramfs/init-lvm selects the LVM init (notes/lvm-encryption-plan.md); it needs
# LVM_SYSROOT=<dir>, an aarch64 SailfishOS tree with usr/sbin/lvm and its libraries
# (scripts/fetch-lvm-sysroot.sh copies them from a running phone). lvm and every library
# it needs (ELF NEEDED, recursively) are copied in at their paths.
set -eu
bb=$1; out=${2:-build/sfos-initramfs.cpio.gz}
cd "$(dirname "$0")/.."
[ -x "$bb" ] || { echo "usage: $0 /path/busybox-static [out.cpio.gz]"; exit 1; }
rm -rf build/initramfs
mkdir -p build/initramfs/bin
install -m 755 "$bb" build/initramfs/bin/busybox
INIT=${INIT:-initramfs/init}
install -m 755 "$INIT" build/initramfs/init
if [ -n "${LVM_SYSROOT:-}" ]; then
  copy() { [ -e "build/initramfs/$1" ] && return 0
    mkdir -p "build/initramfs/$(dirname "$1")"; cp -L "$LVM_SYSROOT/$1" "build/initramfs/$1"; }
  need() {
    for lib in $(readelf -d "$LVM_SYSROOT/$1" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
      for dir in lib64 usr/lib64 lib usr/lib; do
        if [ -e "$LVM_SYSROOT/$dir/$lib" ]; then
          [ -e "build/initramfs/$dir/$lib" ] || { copy "$dir/$lib"; need "$dir/$lib"; }
          break
        fi
      done
    done
  }
  copy usr/sbin/lvm; need usr/sbin/lvm
  copy lib/ld-linux-aarch64.so.1
  echo "lvm and $(find build/initramfs/lib* build/initramfs/usr/lib* -name '*.so*' | wc -l) libraries added"
fi
SPLASH=${SPLASH-initramfs/splash/horizon-sailfish-portrait-memeline-73.png}
[ -z "$SPLASH" ] || magick "$SPLASH" -background black -alpha remove -depth 8 ppm:build/initramfs/splash.ppm
(cd build/initramfs && find . | cpio -o -H newc 2>/dev/null | gzip -9) > "$out"
ls -l "$out"
