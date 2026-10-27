#!/bin/sh
# Stage 1: install the SailfishOS base into /rootfs (runs inside the aarch64 target container).
set -e
R=/rootfs
/port/scripts/repos.sh $R
zypper --root $R -n --gpg-auto-import-keys ref
zypper --root $R -n --cache-dir /port/cache in --no-recommends \
  patterns-sailfish-core-device patterns-sailfish-mw patterns-sailfish-ui \
  patterns-sailfish-device-configuration-common patterns-sailfish-applications patterns-sailfish-cellular-apps \
  jolla-settings-networking-multisim \
  mesa-dri-drivers mesa-libEGL mesa-libGLESv2 mesa-libgbm wayland-egl qt5-plugin-platform-eglfs \
  qt5-qtwayland-wayland_egl qtscenegraph-adaptation usb-moded mce-plugin-libhybris alsa-ucm-conf \
  openssh-server jolla-developer-mode kmod vim-enhanced strace
