#!/bin/sh
# Stage 2 (inside the aarch64 target container): make /rootfs bootable on the Pro1-X.
#  - reinstall the packages whose scriptlets ran before /bin/sh existed (stage 1 bootstrap order)
#  - kernel modules and firmware from the NixOS build (until there are proper RPMs)
#  - droid-config-qx1050 sparse files
#  - bring-up access: sshd, root login with mo's key, usb-moded off (g_cdc is built in)
#  - pack /port/build/sfos-rootfs.tar with numeric owners and xattrs
set -e
R=/rootfs
cd /port

# 1. Scriptlets that failed in stage 1.
pk=$(grep -oE '%[a-z]+\(([^)]+)\) scriptlet failed' notes/stage1.log | sed -E 's/.*\(([^)]+)\).*/\1/' | sort -u)
names=$(for p in $pk; do rpm --root $R -q --qf '%{NAME}\n' "$p" 2>/dev/null; done | sort -u)
echo "stage2: reinstalling: $names"
[ -z "$names" ] || zypper --root $R -n --cache-dir /port/cache in -f --no-recommends $names
chroot $R /sbin/ldconfig

# 2. Kernel modules and firmware.
tar -xf build/modules.tar -C $R/usr
cp -a build/fw/lib/firmware $R/usr/lib/
chroot $R /sbin/depmod -a 7.1.0

# 2b. Port packages built in the Platform SDK (port/packages): qrtr, rmtfs (read-only), tqftpserv.
rpm --root $R -Uvh --replacepkgs build/rpms/*.rpm
chroot $R systemctl enable rmtfs.service tqftpserv.service
chroot $R systemctl mask rmtfs-dir.service

# 3. Device configuration.
cp -a droid-config-qx1050/sparse/. $R/

# 4. Bring-up access.
mkdir -p $R/root/.ssh
cp build/authorized_keys $R/root/.ssh/authorized_keys
chmod 700 $R/root/.ssh; chmod 600 $R/root/.ssh/authorized_keys
mkdir -p $R/etc/ssh/sshd_config.d
echo "PermitRootLogin prohibit-password" > $R/etc/ssh/sshd_config.d/10-bringup.conf
grep -q '^Include /etc/ssh/sshd_config.d' $R/etc/ssh/sshd_config || \
  sed -i '1i Include /etc/ssh/sshd_config.d/*.conf' $R/etc/ssh/sshd_config
chroot $R systemctl enable sshd.socket 2>/dev/null || chroot $R systemctl enable sshd.service
chroot $R systemctl mask usb-moded.service
echo pro1x-sfos > $R/etc/hostname

# 5. Pack.
tar --numeric-owner --xattrs --xattrs-include='*' -C $R -cf build/sfos-rootfs.tar .
ls -la build/sfos-rootfs.tar
