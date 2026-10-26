#!/bin/sh
# Run as root on the phone's NixOS: the NixOS-side twin of sfos-kernel-install.sh. Installs a
# kernel build into the dual-boot layout and arms the one-shot SailfishOS boot.
#   sfos-kernel-install-from-nixos.sh <dir with Image, sm6115-fxtec-pro1x.dtb, modules.tar>
# The modules version is read from modules.tar; a previous SFOS modules directory of the same
# version is kept as /sfos/rootfs/lib/modules/<version>.prev.
set -eu
src=$1
for f in Image sm6115-fxtec-pro1x.dtb modules.tar; do [ -f "$src/$f" ] || { echo "missing $src/$f"; exit 1; }; done
m=/sfos/rootfs/lib/modules
[ -d $m ] || { echo "missing $m"; exit 1; }
v=$(tar -tf "$src/modules.tar" | sed -n 's|^\(\./\)\{0,1\}lib/modules/\([^/]*\)/$|\2|p' | head -1)
[ -n "$v" ] || { echo "no lib/modules/<version>/ in $src/modules.tar"; exit 1; }
install -m 644 "$src/Image" /boot/sfos/Image
install -m 644 "$src/sm6115-fxtec-pro1x.dtb" /boot/sfos/sm6115-fxtec-pro1x.dtb
rm -rf $m/$v.prev
[ -d $m/$v ] && mv $m/$v $m/$v.prev
mkdir -p $m/$v
# --no-same-owner: the tarball carries the build host's uid (see sfos-kernel-install.sh)
tar --no-same-owner -C $m/$v -xf "$src/modules.tar" --strip-components=3 2>/dev/null || tar --no-same-owner -C $m/$v -xf "$src/modules.tar"
[ -f $m/$v/modules.dep ] || { echo "no modules.dep in $m/$v"; exit 1; }
ls $m/$v | head -3
sh "$(dirname "$0")/sfos-boot-once.sh"
sync
echo "installed $src; reboot to test"
