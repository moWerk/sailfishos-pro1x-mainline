#!/bin/sh
# A flashed LVM image (scripts/make-flashable-image.sh) is only as large as its content.
# Give the physical volume the whole userdata partition and the free space to home, then
# grow its ext4 online, as hybris-initrd's root-mount does at first boot.
set -eu
pv=/dev/disk/by-partlabel/userdata
lvm pvresize "$pv"
if [ "$(lvm vgs --noheadings -o vg_free_count sailfish | tr -d ' ')" -gt 0 ]; then
  lvm lvextend -l +100%FREE sailfish/home
fi
# Only a plain ext4 home is grown; an encrypted one is formatted at its full size.
if [ "$(blkid -o value -s TYPE /dev/mapper/sailfish-home)" = ext4 ]; then
  resize2fs /dev/mapper/sailfish-home
fi
mkdir -p /var/lib/qx1050 && touch /var/lib/qx1050/home-grown
