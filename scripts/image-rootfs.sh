#!/bin/sh
# Build a clean, distributable SailfishOS root for the Pro1-X image (runs inside the aarch64
# SDK target container, rootless podman). Replaces stage1-base.sh + stage2-device.sh for images:
# the package set is pinned to the exact name=version-release list of a reference phone, so the
# image matches what was tested, and nothing personal from that phone is copied.
#
# Inputs (mounted at /in):
#   pins.txt          name=version-release, one per line (repo packages)
#   extra.txt         names without a pin (repo picks the version), e.g. mesa-dri-drivers
#   local-rpms/       the port's own RPMs (Platform SDK builds) and hand-fetched ones
#   firmware.tar      /lib/firmware of the reference phone (vendor blobs: never public)
#   modules.tar       kernel modules of the kernel in the boot image
#   zypp-locks        /etc/zypp/locks of the reference phone (keeps the port's builds)
#   authorized_keys   optional: root key for the first-boot gate (remove before handing over)
#   keyboard          optional: xkb variant of fxtec_vndr/pro1x (de, us)
# The port tree is mounted at /port, the root is built in /rootfs, the tar goes to /out.
set -e
R=/rootfs; I=/in; P=/port
V=${SFOS_RELEASE:-5.2.0.22}
step() { echo "== image-rootfs: $* ($(date +%T))"; }

step "repositories ($V, 5.1.0.11 for the held-back audio modules, native-common, dontbeevil)"
add() { zypper --root $R -n rr "$1" >/dev/null 2>&1; zypper --root $R -n ar -f -G "$2" "$1" >/dev/null; }
add jolla             https://releases.jolla.com/releases/$V/jolla/aarch64/
add hotfixes          https://releases.jolla.com/releases/$V/hotfixes/aarch64/
add adaptation-common https://releases.jolla.com/releases/$V/jolla-hw/adaptation-common/aarch64/
add apps              https://releases.jolla.com/jolla-apps/$V/aarch64/
add jolla-5.1         https://releases.jolla.com/releases/5.1.0.11/jolla/aarch64/
add native-common     https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/
add dontbeevil        https://repo.sailfishos.org/obs/nemo:/devel:/hw:/pine:/dontbeevil/sailfish_latest_aarch64/
zypper --root $R -n --gpg-auto-import-keys ref

step "install: $(wc -l < $I/pins.txt) pinned, $(wc -l < $I/extra.txt) unpinned, $(ls $I/local-rpms | wc -l) local"
# One transaction, so the solver sees the port's builds next to the pinned repo versions.
# --oldpackage: some local builds sort below the repo version they replace (qtmultimedia).
# Exit 107 means "a scriptlet failed": expected here (packages installed before /bin/sh
# existed), they are reinstalled below. A rerun after a finished install skips this step.
if [ -f /out/install.done ]; then
  echo "install already done ($(rpm --root $R -qa | wc -l) packages)"
else
  rc=0
  zypper --root $R -n --cache-dir /out/cache --no-gpg-checks in --no-recommends --oldpackage \
    $(cat $I/pins.txt $I/extra.txt) $I/local-rpms/*.rpm > /out/install.log 2>&1 || rc=$?
  if [ $rc -ne 0 ] && [ $rc -ne 107 ]; then
    tail -40 /out/install.log; echo "== image-rootfs: FAILED: zypper install (exit $rc)"; exit 1
  fi
  echo "zypper exit $rc, $(rpm --root $R -qa | wc -l) packages"; touch /out/install.done
fi

step "reinstall packages whose scriptlets ran before /bin/sh existed"
pk=$(grep -oE '%[a-z]+\(([^)]+)\) scriptlet failed' /out/install.log | sed -E 's/.*\(([^)]+)\).*/\1/' | sort -u)
names=$(for p in $pk; do rpm --root $R -q --qf '%{NAME}\n' "$p" 2>/dev/null; done | sort -u)
echo "reinstalling: $names"
[ -z "$names" ] || for n in $names; do
  f=$(ls $I/local-rpms/$n-[0-9]*.rpm 2>/dev/null | head -1)
  if [ -n "$f" ]; then rpm --root $R -Uvh --replacepkgs --oldpackage "$f"
  else zypper --root $R -n --cache-dir /out/cache --no-gpg-checks in -f --no-recommends --oldpackage \
         "$(grep "^$n=" $I/pins.txt || echo $n)"; fi
done
chroot $R /sbin/ldconfig

step "kickstart %post equivalents"
echo -n "aarch64-meego-linux" > $R/etc/rpm/platform
grep -q '^arch = aarch64' $R/etc/zypp/zypp.conf || echo "arch = aarch64" >> $R/etc/zypp/zypp.conf
rm -f $R/var/lib/rpm/__db*
chroot $R rpm --rebuilddb
rm -f $R/.bootstrap
chroot $R /bin/sh -c "
  export LANG=en_US.UTF-8 LC_COLLATE=en_US.UTF-8
  [ -x /usr/bin/oneshot ] && /usr/bin/oneshot --mic
  ssu release $V
  ssu mode 4
"

# Scriptlets that wrote with dd if=/dev/zero met no /dev in the build root and failed silently
# (2>/dev/null). sailfish-devicelock-fpd's lock-code store: without it setting a lock code
# fails with "authentication unavailable".
f=$R/usr/share/lipstick/devicelock/.sfa_lock.enc
if [ ! -f $f ]; then
  dd if=/dev/zero of=$f bs=4096 count=1 2>/dev/null
  chroot $R chown root:disk /usr/share/lipstick/devicelock/.sfa_lock.enc
  chmod 0060 $f
fi

step "kernel modules and firmware"
tar --no-same-owner -xf $I/modules.tar -C $R
chmod -R u+w $R/lib/modules
for v in $(ls $R/lib/modules); do chroot $R /sbin/depmod -a "$v"; done
# Root-owned on purpose: an owner without a passwd entry makes firejail's fcopy abort.
tar --no-same-owner -xf $I/firmware.tar -C $R/lib/

step "device configuration (droid-config-qx1050)"
for f in $P/droid-config-qx1050/kickstart/post/*; do chroot $R /bin/sh < $f; done
cp -a --no-preserve=ownership $P/droid-config-qx1050/sparse/. $R/
# Newer than its last package build (0.1-8): the Bluetooth bridge.
install -m 755 $P/packages/qx1050-voicecall/src/pro1x-voice-call $R/usr/bin/pro1x-voice-call
# The SSC sensors (proximity, light, motion) for sensorfw; hexagonrpcd is enabled by the sparse
# tree. Without it mce takes proximity as covered and blanks the screen in every call.
install -D -m 644 $P/packages/sensorfw-qt5-ssc/config/60-qx1050-ssc.conf \
  $R/etc/sensorfw/sensord.conf.d/60-qx1050-ssc.conf
cp $I/zypp-locks $R/etc/zypp/locks
if [ -s $I/keyboard ]; then
  kb=$(cat $I/keyboard)
  printf "[desktop/lipstick-jolla-home]\n# Image setting: keycaps of this unit\nvariant='%s'\n" "$kb" \
    > $R/etc/dconf/db/vendor.d/zz-keyboard.txt
fi
chroot $R dconf update
echo pro1x-sfos > $R/etc/hostname

step "access for the first-boot gate"
chroot $R systemctl enable sshd.socket 2>/dev/null || chroot $R systemctl enable sshd.service
chroot $R systemctl enable usb-moded.service
chroot $R systemctl enable rmtfs.service tqftpserv.service
# Fingerprint: the TEE supplicant and the daemon (the kernel in the image carries the
# listener and tzmem fixes; never enable these with a kernel that does not).
chroot $R systemctl enable qsee-supplicant.service sailfish-fpd-mafp.service
chroot $R systemctl mask rmtfs-dir.service gpsd.service
if [ -s $I/authorized_keys ]; then
  mkdir -p $R/root/.ssh; cp $I/authorized_keys $R/root/.ssh/authorized_keys
  chmod 700 $R/root/.ssh; chmod 600 $R/root/.ssh/authorized_keys
  mkdir -p $R/etc/ssh/sshd_config.d
  echo "PermitRootLogin prohibit-password" > $R/etc/ssh/sshd_config.d/10-bringup.conf
  grep -q '^Include /etc/ssh/sshd_config.d' $R/etc/ssh/sshd_config || \
    sed -i '1i Include /etc/ssh/sshd_config.d/*.conf' $R/etc/ssh/sshd_config
fi

step "clean: build repositories, caches, identity"
rm -f $R/etc/zypp/repos.d/*.repo
rm -rf $R/var/cache/zypp/* $R/home/.zypp-cache $R/var/log/* $R/tmp/* $R/root/.bash_history
rm -f $R/etc/ssh/ssh_host_* $R/var/lib/systemd/random-seed
: > $R/etc/machine-id

step "manifest and pack"
chroot $R rpm -qa --qf '%{NAME}\t%{VERSION}-%{RELEASE}\t%{ARCH}\n' | sort > /out/image-rpms.tsv
chroot $R systemctl list-unit-files --no-legend --state=enabled,masked | awk '{print $1, $2}' | sort > /out/image-units.txt
tar --numeric-owner --xattrs --xattrs-include='*' -C $R -cf /out/sfos-rootfs.tar .
ls -la /out/sfos-rootfs.tar
step done
