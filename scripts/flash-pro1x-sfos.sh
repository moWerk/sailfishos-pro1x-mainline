#!/bin/sh
# Flash a SailfishOS image set (scripts/make-flashable-image.sh) onto a Pro1-X in fastboot
# mode (power on with Volume Down, or `systemctl reboot bootloader`). DRAFT 2026-10-30.
#   flash-pro1x-sfos.sh [dir]        (default: the directory of this script)
# Writes boot_a and userdata only. ERASES userdata: everything on it, NixOS included.
# The bootloader must be unlocked; vbmeta and dtbo are left as they are.
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

printf 'This erases userdata (all data, NixOS included). Type yes to continue: '
read -r answer
[ "$answer" = yes ] || { echo "aborted"; exit 1; }

flash boot_a sfos-boot.img
flash userdata sailfish.img001
fastboot --set-active=a
echo "done: fastboot reboot"
