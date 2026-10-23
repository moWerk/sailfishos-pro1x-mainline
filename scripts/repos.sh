#!/bin/sh
# Add the package sources of the port to the zypper config under $1 (default /rootfs).
# Release pinned: SailfishOS 5.1.0.11, aarch64.
R=${1:-/rootfs}
V=5.1.0.11
add() { zypper --root "$R" -n ar -f -G "$2" "$1" >/dev/null; }
add jolla             https://releases.jolla.com/releases/$V/jolla/aarch64/
add hotfixes          https://releases.jolla.com/releases/$V/hotfixes/aarch64/
add adaptation-common https://releases.jolla.com/releases/$V/jolla-hw/adaptation-common/aarch64/
add apps              https://releases.jolla.com/jolla-apps/$V/aarch64/
add native-common     https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/
