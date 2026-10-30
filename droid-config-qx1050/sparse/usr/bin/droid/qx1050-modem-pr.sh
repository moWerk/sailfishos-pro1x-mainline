#!/bin/sh
# Mount the modem partition of the booted slot read-only and bind its image/modem_pr next to
# modem.mbn, where tqftpserv looks for the files the modem requests.
fw=/lib/firmware/qcom/sm6115/Fxtec/QX1050
mnt=/run/qx1050/modem
slot=$(sed -n 's/.*androidboot.slot_suffix=\(_[ab]\).*/\1/p' /proc/cmdline)
mkdir -p $mnt $fw/modem_pr
mountpoint -q $mnt || mount -t vfat -o ro,nodev,nosuid,noexec /dev/disk/by-partlabel/modem${slot:-_a} $mnt
mountpoint -q $fw/modem_pr || mount --bind $mnt/image/modem_pr $fw/modem_pr
