# Own kernel for the Pro1-X SailfishOS port (no Nix)

Goal (mo, 2026-10-26): leave NixOS behind as soon as possible; own clean kernel base first,
camera and sensors fixed on it, 7.4 when released.

## Base

- Mainline **v7.3** (released 2026-10-18) + the series in `patches-7.3/` (15 commits).
- The previous base, sm6115-mainline/linux branch `qx1050-7.1`, is plain v7.1 (its tip is
  "Linux 7.1"): all Pro1-X support lived in fxtec-pro1x-nixos `device/kernel/patches/`.
- Tree: `/mnt/storage/sailfish/kernel/w-73`, branch `qx1050-7.3` (worktree of a shallow
  torvalds clone in `../linux`). Not published anywhere yet; mo decides where it lives.

## Rebase 7.1 -> 7.3 (done 2026-10-26)

- `pro1x-egpio`: upstream in 7.3, dropped.
- `sfos-venus`: the sm6115 venus node and the gcc `HW_CTRL_TRIGGER` flag are upstream; only
  the board `&venus` enable remains. Careful: `patch -N` skipped the "reversed" gcc hunk but
  re-added the dtsi node a second time; the commit was rebuilt by hand.
- Board DTS: upstream changed only the touchscreen `irq-gpios` flag (our touch patch has the
  same fix) and added `vbus-supply = <&pmi632_vbus>` on the Type-C port (kept). Each
  patch's DTS hunks were applied to the v7.1 file and 3-way merged onto v7.3 (one
  conflict, the irq-gpios line, resolved to ours).
- Everything else applied with offsets/fuzz only. The same series also applied to master of
  2026-10-25 (7.4 merge window) with the same DTS-only rejects plus three more audio hunks.

## Build

`build-kernel.sh <tree> <out>`: rootless podman, Debian trixie cross gcc, config = arm64
defconfig + `pro1x.fragment` + `sfos.fragment` (copies from fxtec-pro1x-nixos, now owned
here), checked line by line. Release name `7.3.0-qx1050` (`CONFIG_LOCALVERSION`).
On the e15: `~/sailfish-build/qx1050-kernel/{linux,cfg,out-*}`, run with
`BUILD_SLICE=build.slice`. Output = the layout `scripts/sfos-kernel-install.sh` takes
(the install scripts read the modules version from modules.tar since 2026-10-26).

## Still tied to NixOS (next steps to leave it)

- Boot: U-Boot menu and `/boot/sfos` live on the NixOS partition; the one-shot arming writes
  there. Needs an SFOS-owned boot partition/extlinux and the initramfs built outside Nix.
- Firmware in the SFOS rootfs came from the Nix `pro1x-firmware` package (stage2).
- Install path: `sfos-kernel-install.sh` still writes into `/mnt/nixos/boot/sfos`.
