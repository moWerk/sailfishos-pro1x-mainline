#!/bin/sh
# Run as root on SailfishOS (bring-up): install a kernel build from the host into the dual-boot
# layout and arm the next boot with it.
#   sfos-kernel-install.sh <dir with Image, sm6115-fxtec-pro1x.dtb, modules.tar>
# Needs the NixOS filesystem at /mnt/nixos (the initramfs leaves it there).
# modules.tar holds lib/modules/<version> (the kernel's modules); the version is read from it.
# A previous modules directory of the same version is kept as /lib/modules/<version>.prev;
# other versions stay untouched (the old kernel keeps working after a rollback).
set -eu
src=$1
for f in Image sm6115-fxtec-pro1x.dtb modules.tar; do [ -f "$src/$f" ] || { echo "missing $src/$f"; exit 1; }; done
mountpoint -q /mnt/nixos || { echo "/mnt/nixos not mounted"; exit 1; }
v=$(tar -tf "$src/modules.tar" | sed -n 's|^\(\./\)\{0,1\}lib/modules/\([^/]*\)/$|\2|p' | head -1)
[ -n "$v" ] || { echo "no lib/modules/<version>/ in $src/modules.tar"; exit 1; }
install -m 644 "$src/Image" /mnt/nixos/boot/sfos/Image
install -m 644 "$src/sm6115-fxtec-pro1x.dtb" /mnt/nixos/boot/sfos/sm6115-fxtec-pro1x.dtb
rm -rf /lib/modules/$v.prev
[ -d /lib/modules/$v ] && mv /lib/modules/$v /lib/modules/$v.prev
mkdir -p /lib/modules/$v
# --no-same-owner: the tarball carries the build host's uid (no passwd entry here; firejail's
# fcopy aborts on such files and every sandboxed app fails to start).
tar --no-same-owner -C /lib/modules/$v -xf "$src/modules.tar" --strip-components=3 2>/dev/null || tar --no-same-owner -C /lib/modules/$v -xf "$src/modules.tar"
[ -f /lib/modules/$v/modules.dep ] || depmod -b / $v
ls /lib/modules/$v | head -3
DATA=/mnt/nixos sh "$(dirname "$0")/sfos-boot-once.sh"
sync
echo "installed $src; reboot to test"
