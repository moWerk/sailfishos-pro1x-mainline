#!/bin/sh
# ExecStartPre of usb-moded: prepare the configfs gadget usb-moded drives (on hybris
# devices Android's init does this). Upstream functions: rndis.usb0 for developer mode,
# ffs.mtp (FunctionFS, mounted at /dev/mtp for buteo-mtp) for MTP. usb-moded links the
# functions into configs/b.1 and writes UDC itself.
set -u
C=/sys/kernel/config
mountpoint -q $C || mount -t configfs none $C
G=$C/usb_gadget/g1
[ -d $G ] && exit 0
mkdir -p $G
echo 0x2931 > $G/idVendor
echo 0x0A07 > $G/idProduct
echo 0x0200 > $G/bcdUSB
mkdir -p $G/strings/0x409
echo "F(x)tec" > $G/strings/0x409/manufacturer
echo "Pro1-X" > $G/strings/0x409/product
cut -c1-16 /etc/machine-id > $G/strings/0x409/serialnumber
mkdir -p $G/configs/b.1/strings/0x409
echo "usb" > $G/configs/b.1/strings/0x409/configuration
mkdir -p $G/functions/rndis.usb0 $G/functions/ffs.mtp
# usb-moded's developer mode configures rndis0 (the vendor name); the upstream default is usb0
echo "rndis%d" > $G/functions/rndis.usb0/ifname 2>/dev/null || true
mkdir -p /dev/mtp
mountpoint -q /dev/mtp || mount -t functionfs -o uid=0,gid=$(getent group mtp | cut -d: -f3),rmode=0770,fmode=0660 mtp /dev/mtp
exit 0
