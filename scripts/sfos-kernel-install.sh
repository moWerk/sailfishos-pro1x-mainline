#!/bin/sh
# Run as root on SailfishOS (bring-up): install a kernel build from the host into the dual-boot
# layout and arm the next boot with it.
#   sfos-kernel-install.sh <dir with Image, sm6115-fxtec-pro1x.dtb, modules.tar>
# Needs the NixOS filesystem at /mnt/nixos (the initramfs leaves it there).
# modules.tar holds lib/modules/7.1.0 (the kernel's modules); the previous modules directory
# is kept as /lib/modules/7.1.0.prev.
set -eu
src=$1
for f in Image sm6115-fxtec-pro1x.dtb modules.tar; do [ -f "$src/$f" ] || { echo "missing $src/$f"; exit 1; }; done
mountpoint -q /mnt/nixos || { echo "/mnt/nixos not mounted"; exit 1; }
install -m 644 "$src/Image" /mnt/nixos/boot/sfos/Image
install -m 644 "$src/sm6115-fxtec-pro1x.dtb" /mnt/nixos/boot/sfos/sm6115-fxtec-pro1x.dtb
rm -rf /lib/modules/7.1.0.prev
[ -d /lib/modules/7.1.0 ] && mv /lib/modules/7.1.0 /lib/modules/7.1.0.prev
mkdir -p /lib/modules/7.1.0
tar -C /lib/modules/7.1.0 -xf "$src/modules.tar" --strip-components=3 2>/dev/null || tar -C /lib/modules/7.1.0 -xf "$src/modules.tar"
[ -f /lib/modules/7.1.0/modules.dep ] || depmod -b / 7.1.0
ls /lib/modules/7.1.0 | head -3
DATA=/mnt/nixos sh "$(dirname "$0")/sfos-boot-once.sh"
sync
echo "installed $src; reboot to test"
