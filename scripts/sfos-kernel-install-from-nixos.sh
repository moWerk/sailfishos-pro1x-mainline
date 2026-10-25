#!/bin/sh
# Run as root on the phone's NixOS: the NixOS-side twin of sfos-kernel-install.sh. Installs a
# kernel build into the dual-boot layout and arms the one-shot SailfishOS boot.
#   sfos-kernel-install-from-nixos.sh <dir with Image, sm6115-fxtec-pro1x.dtb, modules.tar>
# The previous SFOS modules directory is kept as /sfos/rootfs/lib/modules/7.1.0.prev.
set -eu
src=$1
for f in Image sm6115-fxtec-pro1x.dtb modules.tar; do [ -f "$src/$f" ] || { echo "missing $src/$f"; exit 1; }; done
m=/sfos/rootfs/lib/modules
[ -d $m ] || { echo "missing $m"; exit 1; }
install -m 644 "$src/Image" /boot/sfos/Image
install -m 644 "$src/sm6115-fxtec-pro1x.dtb" /boot/sfos/sm6115-fxtec-pro1x.dtb
rm -rf $m/7.1.0.prev
[ -d $m/7.1.0 ] && mv $m/7.1.0 $m/7.1.0.prev
mkdir -p $m/7.1.0
# --no-same-owner: the tarball carries the build host's uid (see sfos-kernel-install.sh)
tar --no-same-owner -C $m/7.1.0 -xf "$src/modules.tar" --strip-components=3 2>/dev/null || tar --no-same-owner -C $m/7.1.0 -xf "$src/modules.tar"
[ -f $m/7.1.0/modules.dep ] || { echo "no modules.dep in $m/7.1.0"; exit 1; }
ls $m/7.1.0 | head -3
sh "$(dirname "$0")/sfos-boot-once.sh"
sync
echo "installed $src; reboot to test"
