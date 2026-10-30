# LVM layout and home encryption: plan

Status 2026-10-30: **plan, nothing done on the phone.** The move erases `userdata`, NixOS
included, so it waits for the author's go-ahead. The tools are drafts that have not been
booted: `initramfs/init-lvm`, `scripts/make-flashable-image.sh`,
`scripts/flash-pro1x-sfos.sh`, `qx1050-grow-home` in droid-config. The kernel update
package is in `packages/kernel-qx1050/`. Image building is in `notes/flashable-images.md`.

## Where the phone is now

- UFS LUN 0 (`sda`), `userdata` = `sda13`, 241 962 921 984 bytes (225.3 GiB), ext4 labelled
  `nixos`, 18.7 GB used: NixOS (`/nix` 9.4 GB) and the SailfishOS root as the directory
  `/sfos/rootfs` (4.4 GB including a 186 MB `/home`). There is no `/etc/fstab`; `/home` is
  a directory of the root.
- The initramfs bind-mounts that directory as `/` and starts systemd directly
  (`exec switch_root /newroot /sbin/init`).
- Kernel side ready: `BLK_DEV_DM=y`, `DM_CRYPT=y`, AES/XTS built in (`kernel/pro1x.fragment`).
  `lvm2` 2.03.32 and `cryptsetup-libs` are installed; the `cryptsetup` tool is not.
- RAM 7.4 GiB.

## How SailfishOS does it

**Layout.** Jolla's own devices and the official Sony ports put one LVM physical volume on
the data partition, with volume group `sailfish` and the logical volumes `root` and `home`:
- hybris-initrd `sbin/root-mount` (mer-hybris/hybris-initrd, 2026-09-04) activates
  `sailfish` and mounts `ROOTDEV=/dev/sailfish/root`. At the first boot it grows `root` to
  `LVM_ROOT_SIZE` (droid-configs.inc default `lvm_root_size 2500` MB) and `home` to the rest
  (`lvm pvresize`, `lvextend`, `resize2fs`).
- The images are packed the same way: mer-hybris/droid-config-sony-seine
  `kickstart/pack/xqau51/hybris`. It shrinks `root.img` and `home.img`, then
  `pvcreate`/`vgcreate sailfish`/`lvcreate root,home` on a loop file. It dd's both in and
  `img2simg`s the result to `sailfish.img001`, which `flash-config.sh` flashes with
  `flash userdata sailfish.img001`.
- On a Sailfish X Xperia XA2 (SFOS 5, read 2026-10-30): `sailfish-root` 2.4 G on `/`,
  `sailfish-home` 17.6 G on `/home`. Both are mounted by UUID from `/etc/fstab` (`/home`
  through systemd's generated `home.mount`).

**Encryption, Jolla's packages** (`sailfish-device-encryption` 0.11.13, in the `jolla`
repository the phone already uses for 5.2/aarch64; on the XA2 as 0.11.13-1.14.6):
- `home-encryption-preparation.service`: `After=home.mount`,
  `ConditionPathExists=/var/lib/sailfish-device-encryption/encrypt-home`. It copies `/home`
  to `/tmp/home` (or an SD card via `home_copy.conf`), points the users there and runs
  `systemctl stop home.mount`.
- `dbus-org.sailfishos.EncryptionService.service` (`/usr/libexec/sailfish-encryption-service`,
  through udisks2) formats the home device with LUKS and writes `/etc/crypttab`.
- `home-encryption-finish.sh` moves the data back.
- `home-mount-settle.service` runs `udevadm settle` before home is mounted.
- The packages: `-service`, `-settings`, `-unlock-ui`; `-unlock-ui-resources-z1.5` is
  already installed.
- What it needs from a port: `/home` is its own block device, mounted from `/etc/fstab`.

**Encryption, community** (sailfishos-open/sailfish-device-encryption-community, rinigus; the
hybris Pro1-X port by Kabouik ships it: `sailfish-device-encryption-community{,-droid,
-generator,-service,-settings,-wizard}` in its package list):
- For LVM ports, `devices.ini` names `[home_device] device=/dev/sailfish/home_open
  mapper=home_encrypted mount=/home type=device`.
- The home LV is called `home_open`, and `/home` is removed from fstab.
- It needs a patched nemo-qml-plugin-systemsettings (its Setup.md: "PR #7") and
  `HOMEDEV=/dev/sailfish/home_open` in the initrd.
- Its advantage: free-form passwords (argon2) instead of the numeric lock code.
- The alternative for non-LVM ports is a LUKS file container (`type=file`).

**Recommendation:** Jolla's packages first. They match 5.2 exactly, are in the repository
already, and need nothing beyond the standard layout (LV `home`, fstab). The community
variant is the fallback; it would need the LV named `home_open` from the start.

## Target layout on the Pro1-X

| | |
|---|---|
| physical volume | `userdata` (`sda13`), whole partition |
| volume group | `sailfish`, 4 MiB extents |
| `sailfish/root` | 16 GiB ext4 (today 4.2 GB without home; room for OS updates; the hybris default is 2500 MB (`lvm_root_size`) and the XA2 has 2.4 G, but this root carries development packages) |
| `sailfish/home` | the rest, about 209 GiB, ext4, later LUKS |
| `/etc/fstab` | `UUID=<root> / ext4 defaults,noatime 0 1`, `UUID=<home> /home ext4 defaults,noatime 0 2` |

Nothing outside `userdata` changes: `boot_a` gets the new boot image, and `persist`,
`modemst1/2`, `fsg`, `fsc`, `vbmeta` and `dtbo` are not written.

## Boot path changes (`initramfs/init-lvm`, draft)

1. Finds `userdata` by its GPT name in sysfs (`PARTNAME=userdata`, no udev in the
   initramfs).
2. If it is an LVM physical volume, runs `lvm vgchange -a y --sysinit sailfish`, with udev
   synchronisation off so lvm creates the `/dev/mapper` nodes itself, and mounts
   `/dev/mapper/sailfish-root`.
3. Otherwise falls back to today's directory layout (`LABEL=nixos`, `/sfos/rootfs`). So the
   new boot image can be flashed and tested **before** `userdata` is converted.
4. Hands over to **`/sbin/preinit`** instead of systemd, as hybris-initrd's `normal-init`
   does. Today's `exec /sbin/init` skips three things preinit does:
   - the preinit oneshots, which kernel updates use (`add-preinit-oneshot
     /var/lib/platform-updates/flash-bootimg.sh`, see `packages/kernel-qx1050/`);
   - the boot state, including `ACT_DEAD` (`actdead.target`, charging while switched off);
   - the system-time setup.

The initramfs then carries `lvm` and its 14 libraries (glibc, libdevmapper, libsystemd,
libgcrypt...): 5.8 MB compressed, 17 MB unpacked (built 2026-10-30), against a 96 MiB
`boot_a`. Build:

    SSH="ssh ..." scripts/fetch-lvm-sysroot.sh root@<phone> build/lvm-sysroot
    INIT=initramfs/init-lvm LVM_SYSROOT=build/lvm-sysroot \
      scripts/build-initramfs.sh <busybox-static> build/sfos-initramfs-lvm.cpio.gz
    scripts/build-bootimg.sh Image sm6115-fxtec-pro1x.dtb build/sfos-initramfs-lvm.cpio.gz build/sfos-boot-lvm.img

There is no fsck or resize in the initramfs. An image is only as large as its content; the
first boot grows `home` online instead: `qx1050-grow-home.service` runs `lvm pvresize`,
`lvextend -l +100%FREE sailfish/home` and `resize2fs`. It is conditioned on
`/dev/mapper/sailfish-home` and a done marker, so it does nothing on today's layout.

## Steps (each one waits for the author; daytime, the author at the phone)

0. **Backups to the host:** the SailfishOS root with home, and whatever the author wants
   to keep of NixOS (`/nix` and its home, 18.7 GB on the partition in total).
1. **Test preinit and the LVM initramfs on today's layout.** Flash `sfos-boot-lvm.img` to
   `boot_a`, keeping the current image (`fastboot flash boot_a`, or `dd` from SailfishOS as
   so far). The fallback path must boot exactly as now, now through preinit. Check the
   journal for preinit's lines, then charging while off (ACT_DEAD).
   Rollback: flash the previous `boot_a` image.
2. **Kernel package dry run.** Install `kernel-qx1050` once, so that the next boot flashes
   through preinit, reboots and comes up. Rollback: `/boot/sfos-boot.prev.img`.
3. **Capture the root.** With the user session stopped (`systemctl stop user@100000`, over
   USB ssh), run on the phone:
   `tar --numeric-owner --xattrs --xattrs-include='*' -cpf - -C /mnt/nixos/sfos/rootfs .`
   into a file on the host. This is the directory itself, not `/`, which would descend
   into `/mnt/nixos` again.
4. **Build the image on the host:** `sudo scripts/make-flashable-image.sh rootfs.tar
   build/sfos-boot-lvm.img out/pro1x-lvm`. It needs root for the loop device and lvm, is
   restricted to that one loop device, and refuses if the host has a VG named `sailfish`.
5. **Flash:** `systemctl reboot bootloader`, then `out/pro1x-lvm/flash-pro1x-sfos.sh`. It
   writes `boot_a` and `userdata`, refuses `persist`/`modemst1`/`modemst2`/`fsg`/`fsc`,
   checks for an SM6115 phone in fastboot (`product: bengal`) with an unlocked bootloader,
   and asks before it erases. **From here NixOS is gone.**
6. **First boot:** check the journal for the LVM path in the initramfs log, the
   `home.mount` from fstab and `qx1050-grow-home` (home ≈ 209 GiB).
7. **Encryption:**
   - Install `sailfish-device-encryption`, which pulls in `-service`, `-settings` and
     `-unlock-ui`.
   - Set a security code (README row "Lock code": not yet).
   - Settings > Encryption.
   - Check the unlock screen at the next boot.

**Rollback after step 5:** there is no way back to the old partition without a full
image. Flash a `userdata` image of an ext4 labelled `nixos` holding `/sfos/rootfs`
(`mke2fs -t ext4 -L nixos -d <dir>` on the host, then `img2simg`), together with the old
`boot_a` image. Restoring NixOS itself is a NixOS install (fxtec-pro1x-nixos). Stock
Android goes through EDL with persist/modemst1/2/fsg/fsc stripped from rawprogram.

## Open points

- `init-lvm` and lvm without udev in the initramfs have not run on the phone yet; there is
  no aarch64 emulator on the host to try them.
- Whether `sailfish-device-encryption-unlock-ui` draws on the msm DRM display at boot
  (fbdev emulation like the splash, or minui on DRM) is untested.
- The security code: which devicelock implementation the port uses, and whether Settings
  offers encryption without it.
- Off-mode charging (ACT_DEAD) only works once preinit is in the boot path; it gets
  tested in step 1.
- Root size: 16 GiB is a choice (hybris default 2500 MB, XA2 2.4 G).
