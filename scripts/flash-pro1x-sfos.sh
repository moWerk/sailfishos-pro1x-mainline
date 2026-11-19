#!/bin/sh
# Flash a SailfishOS image set (scripts/make-flashable-image.sh) onto a Pro1-X in fastboot
# mode (power on with Volume Down, or `systemctl reboot bootloader`).
#   flash-pro1x-sfos.sh [dir]        (default: the directory of this script)
# Writes boot_a and userdata. ERASES userdata: everything on it (Android, a hybris
# port or NixOS included). The bootloader must be unlocked.
#   PREPARE=<vbmeta.img> flash-pro1x-sfos.sh [dir]
# A phone coming from stock Android or a hybris port also needs, once: vbmeta_a flashed with
# verification off (pass the phone's own vbmeta_a, read before), and dtbo_a erased, so ABL
# lays no Android overlays over the mainline device tree. A phone that ran NixOS has both.
set -eu
dir=${1:-$(cd "$(dirname "$0")" && pwd)}
cd "$dir"

# Never written by any script of this port: device calibration and modem state.
FORBIDDEN="persist modemst1 modemst2 fsg fsc"
flash() {
  for p in $FORBIDDEN; do
    case "$1" in "$p"|"${p}_a"|"${p}_b") echo "refusing to flash $1"; exit 1;; esac
  done
  fastboot flash "$1" "$2"
}

for f in sfos-boot.img sailfish.img001; do [ -f "$f" ] || { echo "missing $dir/$f"; exit 1; }; done
if [ -f md5.lst ]; then md5sum -c md5.lst || { echo "checksum mismatch"; exit 1; }; fi

# The Pro1-X's ABL answers with the platform, "bengal" (SM6115), seen 2026-10-29. Other
# SM6115 phones answer the same, so this only rules out the wrong platform.
product=$(fastboot getvar product 2>&1 | sed -n 's/^product: *//p')
[ "$product" = bengal ] || { echo "not an SM6115 phone in fastboot (product: '${product:-none}')"; exit 1; }
echo "SM6115 (bengal) in fastboot: make sure it is the Pro1-X"
unlocked=$(fastboot getvar unlocked 2>&1 | sed -n 's/^unlocked: *//p')
[ "$unlocked" = yes ] || { echo "bootloader not unlocked (unlocked: '${unlocked:-?}')"; exit 1; }

size=$(fastboot getvar partition-size:userdata 2>&1 | sed -n 's/^partition-size:userdata: *//p')
# Unpacked size from the sparse header: block size (offset 12) times total blocks (offset 16).
set -- $(od -An -tu4 -j12 -N8 sailfish.img001)
img=$(( $1 * $2 ))
echo "userdata: ${size:-?} bytes; image: $img bytes unsparsed"
[ -n "$size" ] && [ "$img" -gt 0 ] && [ "$img" -le $((size)) ] || { echo "image does not fit userdata"; exit 1; }
if [ -n "${PREPARE:-}" ]; then
  [ "$(head -c 4 "$PREPARE")" = AVB0 ] || { echo "PREPARE=$PREPARE is not a vbmeta image"; exit 1; }
  echo "also: vbmeta_a from $PREPARE with verification off, dtbo_a erased"
fi

printf 'This erases userdata (all data on it). Type yes to continue: '
read -r answer
[ "$answer" = yes ] || { echo "aborted"; exit 1; }

if [ -n "${PREPARE:-}" ]; then
  fastboot --disable-verity --disable-verification flash vbmeta_a "$PREPARE"
  fastboot erase dtbo_a
fi
flash boot_a sfos-boot.img
# Erase first (fastboot erases ext4-typed partitions before each flash anyway, and on UFS this
# discards the old blocks), then one flash of the whole sparse image.
fastboot erase userdata
flash userdata sailfish.img001
fastboot --set-active=a
echo "done: fastboot reboot"
