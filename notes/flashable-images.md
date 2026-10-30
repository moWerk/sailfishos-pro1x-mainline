# Flashable images

Status 2026-10-30: **drafts, nothing built or flashed yet.** The scripts are
`scripts/make-flashable-image.sh` and `scripts/flash-pro1x-sfos.sh`; the layout they
produce is in `notes/lvm-encryption-plan.md`.

## What other SailfishOS ports ship

| Port | Format | Install |
|---|---|---|
| Jolla's Sony images (mer-hybris/droid-config-sony-*) | zip: `hybris-boot.img`, `sailfish.img001` (Android sparse image of an LVM PV: VG `sailfish`, LVs `root`, `home`), `flash.sh` / `flash-on-windows.bat`, `md5.lst` | fastboot: `flash boot_a/boot_b`, `flash userdata sailfish.img001` (`sparse/boot/flash-config.sh`), plus the vendor blob the user downloads |
| sailfish-on-dontbeevil (PinePhone etc.) | `rootfs.tar.bz2` from CI | `flash-it.sh`: sudo, `parted`, `mkfs.ext4`, untar onto an SD card; no LVM |
| hybris community ports (droid-hal-configs `pack_package-droid-updater`, e.g. VerdandiTeam victara) | recovery zip: `hybris-boot.img` + rootfs `tar.bz2` + updater-script | TWRP; rootfs unpacked into the Android data partition |

The Pro1-X boots straight from ABL with fastboot available, so the Jolla/Sony format
fits best: same layout as Jolla's devices, one sparse image, no recovery needed.

## The Pro1-X image set

`sudo scripts/make-flashable-image.sh <rootfs.tar> <sfos-boot.img> <out> [root MiB]` writes:

- `sfos-boot.img`: kernel + DTB + LVM initramfs (`scripts/build-bootimg.sh` with
  `INIT=initramfs/init-lvm`, see the plan)
- `sailfish.img001`: `root` (16 GiB by default) and `home` (content + 30 % + 512 MiB),
  sparse; `home` grows to the rest of `userdata` at the first boot
  (`qx1050-grow-home.service`)
- `flash-pro1x-sfos.sh`, `md5.lst`

It needs root: loop devices and device-mapper, which rootless podman cannot give. lvm runs
with `--devices <loop>` only, and the script stops if the host already has a volume group
named `sailfish`. The ext4 images are made with `mke2fs -d` (no mounting), with fixed
UUIDs that also go into the image's `/etc/fstab`.

`flash-pro1x-sfos.sh` (on the host, phone in fastboot):
- checks for an SM6115 phone (`product: bengal`, which this ABL reports; other SM6115
  phones report the same) with an unlocked bootloader;
- verifies `md5.lst` and asks before it erases;
- flashes `boot_a` and `userdata` and sets slot a.
- It refuses `persist`, `modemst1`, `modemst2`, `fsg` and `fsc` by name; no flash list of
  this port ever contains them.
- vbmeta and dtbo stay as the NixOS install left them (verification disabled, dtbo erased).

fastboot sends the sparse image in chunks of the bootloader's `max-download-size`.

## Two kinds of rootfs

- **Distributable:** `build/sfos-rootfs.tar` from `scripts/stage1-base.sh` and
  `scripts/stage2-device.sh`, a clean install with numeric owners and xattrs. Before it
  is shared:
  - remove the bring-up access stage 2 adds (root login with a build key);
  - empty `/etc/machine-id` so it is generated at the first boot (the Wi-Fi MAC is
    derived from it);
  - check the ssh host keys are created at the first boot;
  - leave no SSU credentials or device UID in the image.
- **Moving this phone:** the root captured from the running phone (plan, step 3). It
  holds the author's accounts, keys and device identity. **Personal, never shared.**

## EDL

Not needed for SailfishOS: fastboot writes everything the port uses. EDL (firehose,
rawprogram/patch XML) is only for a return to stock Android. Before any EDL flash, strip
`persist`, `modemst1`, `modemst2`, `fsg` and `fsc` from the rawprogram files and show
the remaining list to the author.

## Open points

- Nothing here has been run.
- The first build needs `sudo` on the host (the author runs it; the script is staged in
  `scripts/`).
- Whether a stage 2 rootfs boots unchanged on the LVM path (fstab, `/home`, the first-boot
  wizard) has to be checked once.
- Kernel updates after flashing come through `packages/kernel-qx1050`.
