#!/bin/sh
# Build the SailfishOS boot image for boot_a: ABL loads it directly, no second bootloader
# (the SailfishOS and postmarketOS convention on ABL phones).
#   build-bootimg.sh Image sm6115-fxtec-pro1x.dtb initramfs.cpio.gz [out.img]
# Header v0, kernel = gzipped Image with the DTB appended; offsets as in
# fxtec-pro1x-nixos lib/bootimg.nix (proven on this ABL). ABL appends its own
# androidboot.* arguments. Needs mkbootimg (android-tools).
set -eu
[ $# -ge 3 ] || { echo "usage: $0 Image dtb initramfs.cpio.gz [out.img]"; exit 1; }
image=$1; dtb=$2; initrd=$3; out=${4:-build/sfos-boot.img}
# Quiet: no kernel console on the screen (serial only), no Tux logo, no cursor, so the
# initramfs splash stays until lipstick draws. Add console=tty1 to see the boot log.
cmdline=${CMDLINE:-"console=ttyMSM0,115200n8 loglevel=3 log_buf_len=1M lsm=landlock,yama,bpf logo.nologo vt.global_cursor_default=0"}
t=$(mktemp -d); trap 'rm -rf "$t"' EXIT
gzip -n -9 -c "$image" > "$t/Image.gz"
cat "$t/Image.gz" "$dtb" > "$t/Image.gz-dtb"
mkbootimg --header_version 0 --pagesize 4096 --base 0x00000000 \
  --kernel_offset 0x00008000 --ramdisk_offset 0x01000000 --second_offset 0x00f00000 \
  --tags_offset 0x00000100 --kernel "$t/Image.gz-dtb" --ramdisk "$initrd" \
  --cmdline "$cmdline" -o "$out"
ls -l "$out"
