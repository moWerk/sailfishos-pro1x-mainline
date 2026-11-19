#!/bin/sh
# Build the SailfishOS bring-up initramfs: busybox (static, aarch64) + initramfs/init.
#   build-initramfs.sh /path/to/static/busybox [out.cpio.gz]
# SPLASH=image (1080x2160; PNG, BMP or PPM) is the boot splash init draws on fb0; default
# initramfs/splash/horizon-sailfish-portrait-memeline-73.png, SPLASH= for none. Needs ImageMagick (magick).
# The staged tree is build/initramfs; the archive goes to build/sfos-initramfs.cpio.gz.
# INIT=initramfs/init-lvm selects the LVM init (notes/lvm-encryption-plan.md); it needs
# LVM_SYSROOT=<dir>, an aarch64 SailfishOS tree with usr/sbin/lvm, e2fsck and their libraries
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
  # e2fsck for the root check before the mount (as hybris-initrd's root-mount does)
  for e in usr/sbin/e2fsck sbin/e2fsck; do
    if [ -e "$LVM_SYSROOT/$e" ]; then copy $e; need $e
      [ $e = usr/sbin/e2fsck ] || { mkdir -p build/initramfs/usr/sbin; ln -sf /$e build/initramfs/usr/sbin/e2fsck; }
      break; fi
  done
  copy lib/ld-linux-aarch64.so.1
  # The RTC starts at 1970 until timed corrects it: without this e2fsck stops at a
  # last-mount time "in the future".
  mkdir -p build/initramfs/etc
  printf '[options]\n\tbroken_system_clock = 1\n' > build/initramfs/etc/e2fsck.conf
  echo "lvm, e2fsck and $(find build/initramfs/lib* build/initramfs/usr/lib* -name '*.so*' | wc -l) libraries added"
fi
# RESCUE=1 builds the rescue image: init-lvm sees /rescue and starts a shell on USB instead
# of booting (VG active, nothing mounted).
[ "${RESCUE:-0}" = 1 ] && : > build/initramfs/rescue
SPLASH=${SPLASH-initramfs/splash/horizon-sailfish-portrait-memeline-73.png}
[ -z "$SPLASH" ] || magick "$SPLASH" -background black -alpha remove -depth 8 ppm:build/initramfs/splash.ppm
(cd build/initramfs && find . | cpio -o -H newc 2>/dev/null | gzip -9) > "$out"
ls -l "$out"
