# Boot

Since 2026-10-29 the phone boots SailfishOS straight from the Android bootloader (ABL), the
way SailfishOS ports (hybris and native) and postmarketOS's per-device phone ports do it on
ABL phones: a plain Android boot image in `boot_a`, no second bootloader.

## Boot image

`scripts/build-bootimg.sh Image dtb initramfs.cpio.gz [out.img]`: header v0, kernel = the
gzipped Image with the DTB appended, initramfs from `scripts/build-initramfs.sh`. Offsets as in
fxtec-pro1x-nixos `lib/bootimg.nix` (page size 4096, base 0, kernel 0x8000, ramdisk
0x1000000, second 0xf00000, tags 0x100). ABL appends its own arguments (`androidboot.*`,
`androidboot.slot_suffix=_a`, `rootwait ro init=/init`); the initramfs ignores them.

The initramfs mounts the partition labelled `nixos` (userdata) and switches to the directory
`/sfos/rootfs` on it. On any failure it powers off: a reboot would come straight back.

## Flashing and recovery

- From SailfishOS: `systemctl reboot bootloader` lands in fastboot (about 12 s).
- Without a running system: power on with Volume Down.
- `fastboot flash boot_a sfos-boot.img`, `fastboot reboot`.
- This ABL refuses `fastboot boot` (RAM boot): "unknown command". Every test is a flash.
  Keep the previous image to flash back.
- The bootloader is unlocked; vbmeta has verification disabled and dtbo is erased (from the
  NixOS install). Neither is touched.

## A/B slot

ABL counts boot attempts per slot and stops booting one that is never marked successful.
`qx1050-boot-success.service` runs `qbootctl -m` (package `qbootctl`, linux-msm 0.2.2) after
every boot; fxtec-pro1x-nixos does the same. Slot b is unused (the NixOS project notes that
it does not boot on this phone; not investigated).

## Kernel updates

Build the kernel (`kernel/build-kernel.sh`), install the modules into the rootfs, build the
boot image and flash `boot_a`. On the phone the image can also be written with
`dd if=sfos-boot.img of=/dev/disk/by-partlabel/boot_a` from SailfishOS. The SailfishOS
convention for packaged kernel updates (`/boot/hybris-boot.img` plus
`platform-updates/flash-bootimg.sh` at the next boot) is not wired up yet.

## Dual boot with NixOS (option)

The earlier setup stays possible for anyone who wants both systems: fxtec-pro1x-nixos's
U-Boot in `boot_a`, with NixOS's extlinux.conf on userdata. `scripts/sfos-boot-once.sh` arms
a single SailfishOS boot from it (an extlinux entry with this kernel and initramfs); the
initramfs restores NixOS's extlinux.conf before switching, so the next boot is NixOS again.
The U-Boot menu (Volume Down) keeps earlier entries selectable.

## Still open

- The rootfs is still a directory on the NixOS-formatted userdata partition. The SailfishOS
  convention is LVM on userdata (`sailfish/root`, `sailfish/home`); that layout is also what
  home encryption needs (LUKS on the home volume). Moving to it erases userdata.
- Boot splash (yamui in the initramfs) and a quiet console.
- Firmware in the rootfs came from the Nix `pro1x-firmware` package.
