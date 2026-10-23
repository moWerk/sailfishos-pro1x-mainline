#!/bin/sh
# Run as root on the phone's NixOS. Installs (or replaces) the SailfishOS root directory.
#   install-on-nixos.sh /path/sfos-rootfs.tar /path/sfos-initramfs.cpio.gz
# Writes only inside the existing ext4: /sfos/rootfs and /boot/sfos. No partition is touched.
set -eu
tarball=$1; initramfs=$2
[ -f "$tarball" ] && [ -f "$initramfs" ] || { echo "usage: $0 rootfs.tar initramfs.cpio.gz"; exit 1; }

mkdir -p /sfos /boot/sfos
if [ -d /sfos/rootfs ]; then
  rm -rf /sfos/rootfs.old
  mv /sfos/rootfs /sfos/rootfs.old
fi
mkdir /sfos/rootfs
tar --numeric-owner --xattrs --xattrs-include='*' -xpf "$tarball" -C /sfos/rootfs

# File capabilities were set inside a user namespace on the build host; set them again here.
chroot /sfos/rootfs /usr/bin/rpm --setcaps -a 2>/dev/null || echo "warning: rpm --setcaps failed"

# Wi-Fi: turn NixOS's NetworkManager keyfiles into connman provisioning files, on the
# phone only (the credentials never leave it).
mkdir -p /sfos/rootfs/var/lib/connman
for f in /etc/NetworkManager/system-connections/*.nmconnection; do
  [ -f "$f" ] || continue
  ssid=$(sed -n 's/^ssid=//p' "$f" | head -1); psk=$(sed -n 's/^psk=//p' "$f" | head -1)
  [ -n "$ssid" ] || continue
  n=$(basename "$f" .nmconnection | tr -c 'A-Za-z0-9_\n-' '_')
  {
    echo "[service_$n]"; echo "Type = wifi"; echo "Name = $ssid"
    [ -n "$psk" ] && echo "Passphrase = $psk"
  } > /sfos/rootfs/var/lib/connman/nixos-$n.config
  chmod 600 /sfos/rootfs/var/lib/connman/nixos-$n.config
done

install -m 644 "$initramfs" /boot/sfos/sfos-initramfs.cpio.gz
rm -rf /sfos/rootfs.old
sync
echo "installed: $(du -sh /sfos/rootfs | cut -f1) in /sfos/rootfs"
