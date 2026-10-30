#!/bin/sh
# Build a fastboot-flashable image set for the Pro1-X in Jolla's layout: userdata becomes
# the LVM physical volume of volume group "sailfish" with the logical volumes root and
# home, packed as one Android sparse image (sailfish.img001), as Jolla's Sony images
# (droid-config-sony-*, kickstart/pack/*/hybris) do. DRAFT 2026-10-30, not yet flashed.
#
#   sudo make-flashable-image.sh <rootfs.tar> <sfos-boot.img> <out dir> [root size MiB]
#
# rootfs.tar: a SailfishOS root with numeric owners and xattrs (stage2-device.sh writes
#   build/sfos-rootfs.tar; for the move of an existing phone, see
#   notes/lvm-encryption-plan.md). Its /home goes to the home volume.
# sfos-boot.img: from scripts/build-bootimg.sh with the LVM initramfs (initramfs/init-lvm).
# Root: loop devices and device-mapper need it (rootless podman cannot); everything is
# confined to the out dir and one loop device, and lvm is restricted to that device.
# Output: <out>/sfos-boot.img, sailfish.img001, flash-pro1x-sfos.sh, md5.lst.
set -eu
[ "$(id -u)" -eq 0 ] || { echo "run as root (loop devices, device-mapper)"; exit 1; }
[ $# -ge 3 ] || { echo "usage: $0 rootfs.tar sfos-boot.img out-dir [root-MiB]"; exit 1; }
tarball=$1; bootimg=$2; out=$3; root_mib=${4:-16384}
here=$(cd "$(dirname "$0")" && pwd)
[ "$(head -c 8 "$bootimg")" = "ANDROID!" ] || { echo "$bootimg is not a boot image"; exit 1; }
if lvm vgs sailfish >/dev/null 2>&1; then
  echo "this host already has a volume group named sailfish; refusing"; exit 1
fi

mkdir -p "$out"
work=$(mktemp -d "$out/.work.XXXXXX")
loop=
cleanup() {
  [ -n "$loop" ] && { lvm vgchange -a n --devices "$loop" sailfish >/dev/null 2>&1 || :; losetup -d "$loop" || :; }
  rm -rf "$work"
}
trap cleanup EXIT

echo "step: unpacking $tarball"
mkdir "$work/root" "$work/home"
tar --numeric-owner --xattrs --xattrs-include='*' -xpf "$tarball" -C "$work/root"
[ -e "$work/root/sbin/preinit" ] || { echo "no /sbin/preinit in the rootfs"; exit 1; }
# /home becomes its own volume: move its contents, keep the empty mount point
if [ -d "$work/root/home" ]; then
  (cd "$work/root/home" && find . -mindepth 1 -maxdepth 1 -exec mv {} "$work/home/" \;)
fi
mkdir -p "$work/root/home"
# The image is not tied to the NixOS layout any more
rmdir "$work/root/mnt/nixos" 2>/dev/null || :

root_uuid=$(cat /proc/sys/kernel/random/uuid)
home_uuid=$(cat /proc/sys/kernel/random/uuid)
# Jolla's devices mount both by UUID from /etc/fstab (systemd's home.mount), which is
# what sailfish-device-encryption expects to find and replace.
cat > "$work/root/etc/fstab" <<EOF
UUID=$root_uuid  /      ext4  defaults,noatime  0 1
UUID=$home_uuid  /home  ext4  defaults,noatime  0 2
EOF

echo "step: filesystem images"
home_kib=$(du -sk "$work/home" | cut -f1)
home_mib=$(( home_kib / 1024 * 13 / 10 + 512 ))   # content + 30 % + 512 MiB; grown at first boot
# whole 4 MiB extents, so each volume holds its image exactly
home_mib=$(( (home_mib + 3) / 4 * 4 )); root_mib=$(( (root_mib + 3) / 4 * 4 ))
truncate -s "${root_mib}M" "$work/root.img"
truncate -s "${home_mib}M" "$work/home.img"
mke2fs -q -t ext4 -L root -U "$root_uuid" -d "$work/root" "$work/root.img"
mke2fs -q -t ext4 -L home -U "$home_uuid" -d "$work/home" "$work/home.img"
e2fsck -fn "$work/root.img" >/dev/null && e2fsck -fn "$work/home.img" >/dev/null

echo "step: LVM image (root ${root_mib} MiB, home ${home_mib} MiB)"
# 4 MiB extents, data from 1 MiB; 8 MiB slack for metadata and rounding
truncate -s $(( root_mib + home_mib + 8 ))M "$work/sailfish.img"
loop=$(losetup --find --show "$work/sailfish.img")
lvm pvcreate -q --devices "$loop" --dataalignment 1m "$loop"
lvm vgcreate -q --devices "$loop" -s 4m sailfish "$loop"
lvm lvcreate -q --devices "$loop" -y -L "${root_mib}m" -n root sailfish
lvm lvcreate -q --devices "$loop" -y -l 100%FREE -n home sailfish
dd if="$work/root.img" of=/dev/mapper/sailfish-root bs=4M conv=fsync status=none
dd if="$work/home.img" of=/dev/mapper/sailfish-home bs=4M conv=fsync status=none
lvm lvs --devices "$loop" sailfish
lvm vgchange -q -a n --devices "$loop" sailfish
losetup -d "$loop"; loop=

echo "step: sparse image"
img2simg "$work/sailfish.img" "$out/sailfish.img001"
cp "$bootimg" "$out/sfos-boot.img"
cp "$here/flash-pro1x-sfos.sh" "$out/"
(cd "$out" && md5sum sfos-boot.img sailfish.img001 > md5.lst)
ls -l "$out"
