#!/bin/sh
# Stage 2 (inside the aarch64 target container): make /rootfs bootable on the Pro1-X.
#  - reinstall the packages whose scriptlets ran before /bin/sh existed (stage 1 bootstrap order)
#  - kernel modules and firmware from the NixOS build (until there are proper RPMs)
#  - droid-config-qx1050 sparse files
#  - bring-up access: sshd, root login with the key in build/authorized_keys
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

# 1b. What mic's kickstart %post does (taken from the nabu/pinephone kickstarts):
# rpm arch, rebuilt rpm db, out of bootstrap mode, oneshot jobs, ssu release mode.
echo -n "aarch64-meego-linux" > $R/etc/rpm/platform
grep -q '^arch = aarch64' $R/etc/zypp/zypp.conf || echo "arch = aarch64" >> $R/etc/zypp/zypp.conf
rm -f $R/var/lib/rpm/__db*
chroot $R rpm --rebuilddb
rm -f $R/.bootstrap
chroot $R /bin/sh -c '
  export LANG=en_US.UTF-8 LC_COLLATE=en_US.UTF-8
  UID_MIN=$(grep "^UID_MIN" /etc/login.defs | tr -s " " | cut -d " " -f2)
  DEVICEUSER=$(getent passwd $UID_MIN | sed "s/:.*//")
  echo "stage2: device user: $DEVICEUSER"
  if [ -x /usr/bin/oneshot ]; then
    /usr/bin/oneshot --mic
    [ -n "$DEVICEUSER" ] && su -c "/usr/bin/oneshot --mic" $DEVICEUSER
  fi
  ssu release 5.1.0.11
  ssu mode 4
'

# 2. Kernel modules and firmware.
# Everything copied in must end up root-owned: a file whose owner has no passwd entry on the
# device makes firejail's fcopy abort, and with it every sandboxed app (camera, browser).
tar --no-same-owner -xf build/modules.tar -C $R
chmod -R u+w $R/lib/modules
# venus firmware comes from linux-firmware, not the Fxtec image: scripts/fetch-venus-fw.sh
[ -f build/fw/lib/firmware/qcom/venus-6.0/venus.mbn ] || { echo 'stage2: run scripts/fetch-venus-fw.sh first'; exit 1; }
cp -a --no-preserve=ownership build/fw/lib/firmware $R/lib/
for v in $(ls $R/lib/modules); do chroot $R /sbin/depmod -a "$v"; done

# 2b. Port packages built in the Platform SDK (port/packages): qrtr, rmtfs (read-only), tqftpserv.
rpm --root $R -Uvh --replacepkgs build/rpms/*.rpm
# The top-menu flashlight toggle; its provider (sailfish-flashlight-provider) is the port's
# sysfs-flashlight from build/rpms, so it can only come after them.
zypper --root $R -n --cache-dir /port/cache in --no-recommends jolla-settings-system-flashlight
chroot $R systemctl enable rmtfs.service tqftpserv.service
chroot $R systemctl mask rmtfs-dir.service

# 3. Device configuration.
for f in droid-config-qx1050/kickstart/post/*; do chroot $R /bin/sh < $f; done
cp -a --no-preserve=ownership droid-config-qx1050/sparse/. $R/

# 4. Bring-up access.
mkdir -p $R/root/.ssh
cp build/authorized_keys $R/root/.ssh/authorized_keys
chmod 700 $R/root/.ssh; chmod 600 $R/root/.ssh/authorized_keys
mkdir -p $R/etc/ssh/sshd_config.d
echo "PermitRootLogin prohibit-password" > $R/etc/ssh/sshd_config.d/10-bringup.conf
grep -q '^Include /etc/ssh/sshd_config.d' $R/etc/ssh/sshd_config || \
  sed -i '1i Include /etc/ssh/sshd_config.d/*.conf' $R/etc/ssh/sshd_config
chroot $R systemctl enable sshd.socket 2>/dev/null || chroot $R systemctl enable sshd.service
chroot $R systemctl enable usb-moded.service
echo pro1x-sfos > $R/etc/hostname

# 5. Pack.
tar --numeric-owner --xattrs --xattrs-include='*' -C $R -cf build/sfos-rootfs.tar .
ls -la build/sfos-rootfs.tar
