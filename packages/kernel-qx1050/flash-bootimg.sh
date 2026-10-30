#!/bin/sh
# Write /boot/sfos-boot.img to the boot slot the phone runs from, verified, with the
# previous image kept in /boot/sfos-boot.prev.img. Run by SailfishOS's preinit once
# after a kernel-qx1050 update (add-preinit-oneshot), or by hand as root followed by a
# reboot. Exits non-zero, with the old image restored, if the written image differs.
set -eu
img=/boot/sfos-boot.img
log() { echo "flash-bootimg: $*"; echo "flash-bootimg: $*" > /dev/kmsg 2>/dev/null || :; }

[ -f "$img" ] || { log "no $img"; exit 1; }
# Android boot image magic
[ "$(head -c 8 "$img")" = "ANDROID!" ] || { log "$img is not a boot image"; exit 1; }

slot=$(sed -n 's/.*androidboot.slot_suffix=\(_[ab]\).*/\1/p' /proc/cmdline)
name=boot${slot:-_a}
# preinit runs before udev: no /dev/disk/by-partlabel yet, so read the GPT name in sysfs
part=
for u in /sys/class/block/*/uevent; do
  if grep -qx "PARTNAME=$name" "$u"; then part=/dev/$(sed -n 's/^DEVNAME=//p' "$u"); break; fi
done
[ -n "$part" ] && [ -b "$part" ] || { log "no partition $name"; exit 1; }

size=$(stat -c %s "$img")
psize=$(blockdev --getsize64 "$part")
[ "$size" -le "$psize" ] || { log "$img ($size) larger than $part ($psize)"; exit 1; }

want=$(sha256sum < "$img")
if [ "$(head -c "$size" "$part" | sha256sum)" = "$want" ]; then
  log "$part already holds this image"
  exit 0
fi

dd if="$part" of=/boot/sfos-boot.prev.img bs=1M 2>/dev/null
dd if="$img" of="$part" bs=1M conv=fsync 2>/dev/null
if [ "$(head -c "$size" "$part" | sha256sum)" = "$want" ]; then
  log "$img written to $part and verified"
  exit 0
fi
log "verify failed on $part, restoring the previous image"
dd if=/boot/sfos-boot.prev.img of="$part" bs=1M conv=fsync 2>/dev/null
exit 1
