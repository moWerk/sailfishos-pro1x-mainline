#!/bin/sh
# Run as root on the phone's NixOS: boot SailfishOS ONCE at the next reboot.
# Keeps NixOS's extlinux.conf as extlinux.nixos.conf, writes a copy whose DEFAULT is
# the SFOS entry (appended at the end, so the NixOS entries and the prev*.conf files of
# the U-Boot menu stay as they are). The SFOS initramfs moves the NixOS file back
# before SFOS starts, so every later reboot is NixOS again.
# Kernel, DTB and the device part of the cmdline are copied from NixOS's default entry.
set -eu
# DATA: where the NixOS ext4 is mounted ("" on NixOS itself; e.g. /mnt/nixos from SailfishOS)
DATA=${DATA:-}
b=$DATA/boot/extlinux
src=$b/extlinux.conf
[ -f $DATA/boot/sfos/sfos-initramfs.cpio.gz ] || { echo "missing $DATA/boot/sfos/sfos-initramfs.cpio.gz"; exit 1; }
[ -d $DATA/sfos/rootfs/usr ] || { echo "missing $DATA/sfos/rootfs"; exit 1; }
[ -f $b/extlinux.nixos.conf ] && { echo "already armed (extlinux.nixos.conf exists)"; exit 0; }

def=$(awk '$1 == "DEFAULT" { print $2; exit }' $src)
entry() { awk -v l="$def" '$1 == "LABEL" { on = ($2 == l) } on && $1 == "'"$1"'" { $1 = ""; sub(/^ /, ""); print; exit }' $src; }
kernel=$(entry LINUX); fdt=$(entry FDT); append=$(entry APPEND)
[ -n "$kernel" ] && [ -n "$fdt" ] && [ -n "$append" ] || { echo "cannot parse entry $def"; exit 1; }
# The SFOS kernel (sfos.fragment on top of NixOS's config), when installed, replaces NixOS's.
if [ -f $DATA/boot/sfos/Image ] && [ -f $DATA/boot/sfos/sm6115-fxtec-pro1x.dtb ]; then
  kernel=../sfos/Image; fdt=../sfos/sm6115-fxtec-pro1x.dtb
fi
# Drop the NixOS-only arguments; keep consoles, USB gadget MACs, lsm.
append=$(echo "$append" | tr ' ' '\n' | grep -vE '^(init=|root=|systemd\.log_target=)' | tr '\n' ' ')

cp -p $src $b/extlinux.nixos.conf
{
  sed 's/^DEFAULT .*/DEFAULT sfos/' $b/extlinux.nixos.conf
  echo
  echo "LABEL sfos"
  echo "  MENU LABEL SailfishOS (mainline, one-shot)"
  echo "  LINUX $kernel"
  echo "  INITRD ../sfos/sfos-initramfs.cpio.gz"
  echo "  APPEND ${append}${SFOS_EXTRA_APPEND:-}"
  echo "  FDT $fdt"
} > $b/.extlinux.sfos.tmp
mv -f $b/.extlinux.sfos.tmp $src
sync
echo "armed: next boot is SailfishOS once; then NixOS again"
