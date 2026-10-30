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
convention for packaged kernel updates (a boot image in `/boot` plus
`platform-updates/flash-bootimg.sh` run by preinit at the next boot) is drafted in
`packages/kernel-qx1050/` (2026-10-30); it needs the initramfs to hand over to
`/sbin/preinit`, which `initramfs/init-lvm` does.

## Dual boot with NixOS (option)

The earlier setup stays possible for anyone who wants both systems: fxtec-pro1x-nixos's
U-Boot in `boot_a`, with NixOS's extlinux.conf on userdata. `scripts/sfos-boot-once.sh` arms
a single SailfishOS boot from it (an extlinux entry with this kernel and initramfs); the
initramfs restores NixOS's extlinux.conf before switching, so the next boot is NixOS again.
The U-Boot menu (Volume Down) keeps earlier entries selectable.

## Splash

The firmware's splash (Android logo) is signed; the `splash` partition is ignored on this
phone (tested 2026-10-29 with the Pro¹'s `DDPH` v1 layout, images at 0x4000 and 0x800000,
and with Qualcomm's UEFI layout, 0x4000 zero bytes then a BMP; both left the Android logo,
and the partition is zeroed again). The port's own splash: the initramfs draws a PPM with
busybox `fbsplash` on `/dev/fb0` (DRM fbdev emulation, 1080x2160, 32 bpp, stride 4352)
at about 2 s; with the kernel console on serial only, nothing overwrites it until lipstick
takes the display. See README "Custom boot splash". A few frames of glitches remain while
msm DRM takes over the display from the firmware.

## Still open

- The rootfs is still a directory on the NixOS-formatted userdata partition. The SailfishOS
  convention is LVM on userdata (`sailfish/root`, `sailfish/home`); that layout is also what
  home encryption needs (LUKS on the home volume). Moving to it erases userdata. Plan and
  draft tools: `notes/lvm-encryption-plan.md`, `notes/flashable-images.md`.
- Glitches between the Android logo and the splash.
- Reboot: about 8 s of black screen. Traced 2026-10-29: systemd's shutdown takes 6.4 s with
  no timeout or hanging unit (user session 3.8 s: apps, boosters, Telepathy, lipstick; then
  mce, wpa_supplicant, connman, which needs about 1 s to exit), then the reset and firmware.
- Firmware in the rootfs came from the Nix `pro1x-firmware` package.
