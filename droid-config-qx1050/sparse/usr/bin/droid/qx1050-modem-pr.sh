#!/bin/sh
# Mount the modem partition of the booted slot read-only and bind its image/modem_pr next to
# modem.mbn, where tqftpserv looks for the files the modem requests.
fw=/lib/firmware/qcom/sm6115/Fxtec/QX1050
mnt=/run/qx1050/modem
slot=$(sed -n 's/.*androidboot.slot_suffix=\(_[ab]\).*/\1/p' /proc/cmdline)
dev=/dev/disk/by-partlabel/modem${slot:-_a}
mkdir -p $mnt $fw/modem_pr
# Early in boot udev may not have created the partlabel link yet (the first boot with this unit
# failed with mount's exit 32 at 2 s): wait for it.
for i in $(seq 1 30); do [ -e $dev ] && break; sleep 1; done
mountpoint -q $mnt || mount -t vfat -o ro,nodev,nosuid,noexec $dev $mnt
mountpoint -q $fw/modem_pr || mount --bind $mnt/image/modem_pr $fw/modem_pr
