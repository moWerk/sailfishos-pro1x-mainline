# kernel-qx1050

Kernel updates as an RPM, the SailfishOS way (hybris-initrd `droid-hal-device-img-boot.inc`):
the package installs the boot image and the modules, and its `%post` schedules
`/var/lib/platform-updates/flash-bootimg.sh` with `add-preinit-oneshot`. At the next boot
SailfishOS's `/sbin/preinit` runs it before systemd, then reboots into the new kernel.

**Status 2026-10-30:** built once (`kernel-qx1050-7.3.0-1`, 34 MB, from the running
kernel's boot image and modules), not installed. The scheduled flash only happens once
the initramfs hands over to `/sbin/preinit` (`initramfs/init-lvm`;
`notes/lvm-encryption-plan.md`, step 1). Until then, run the script by hand after an
install: `sh /var/lib/platform-updates/flash-bootimg.sh && reboot`.

## Build

    packages/kernel-qx1050/prepare-input.sh build/sfos-boot.img <modules.tar>
    ~/sailfish/scripts/sfos-mb2-build.sh <port>/packages/kernel-qx1050

- `sfos-boot.img` comes from `scripts/build-bootimg.sh`.
- `modules.tar` holds `lib/modules/7.3.0-qx1050/`, as the kernel build host writes it.
- Both go to `input/` (not in git).
- Raise `Release` for every new build: the modules directory keeps the name
  `7.3.0-qx1050`, so the package version is what tells builds apart.

## flash-bootimg.sh

- Takes the running slot from `androidboot.slot_suffix` on the command line and finds the
  partition by its GPT name in sysfs (preinit runs before udev).
- Checks the boot image magic and that the image fits the partition.
- Keeps the old content in `/boot/sfos-boot.prev.img`.
- Writes with `conv=fsync` and compares the SHA-256 of what landed. On a mismatch it
  restores the old image and fails; preinit then keeps the job as `*.failed`.
- If the slot already holds the image, it does nothing.

Slot b is not written: it does not boot on this phone (the NixOS project's note, not
investigated here).
