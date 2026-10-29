# qbootctl

[linux-msm/qbootctl](https://github.com/linux-msm/qbootctl) tag 0.2.2 (commit in
`SOURCE`), unchanged, packaged for the port. The phone boots SailfishOS straight from
ABL, so the running slot is marked successful after every boot
(`qx1050-boot-success.service` in droid-config), as fxtec-pro1x-nixos does with
`qbootctl -m`.

`src/compat/scsi/scsi_bsg_ufs.h` is the Linux 7.3 UAPI header (GPL-2.0 WITH
Linux-syscall-note): the SailfishOS SDK target's kernel headers predate it.
