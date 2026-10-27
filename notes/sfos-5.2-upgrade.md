# SailfishOS 5.1.0.11 -> 5.2.0.22 on the mainline port (2026-10-27)

Done on the device itself, kernel 7.3.0-qx1050 unchanged. Took about 25 minutes over Wi-Fi.

1. Snapshot: `cp -a /mnt/nixos/sfos/rootfs /mnt/nixos/sfos/rootfs-5.1.0.11` (3.0 GB). Busybox cp
   drops xattrs; no file in the rootfs carries capabilities (`getcap -r /usr` empty), so that is fine.
2. `pkcon install zypper` (not in the image).
3. Lock every package that is not a Jolla build (release without `jolla`): our own builds,
   the dontbeevil (bso) and chum ones. Jolla 5.2 ships ofono 1.29, its own pulseaudio and
   qt5-qtmultimedia; a dup would swap or downgrade ours. Vendor is no filter: Jolla packages say
   `meego`. List (33): alsa-ucm-conf ell gstreamer1.0-plugins-good hexagonrpc libcamera libcamera-tools libevent libqmi libqmi-utils libqrtr-glib libssc libssc-tools mce-plugin-libhybris mesa-dri-drivers mesa-libEGL mesa-libgbm mesa-libGLESv2 ofono protobuf-c pulseaudio qrtr qt5-qtdeclarative-import-multimedia qt5-qtmultimedia qt5-qtmultimedia-gsttools qt5-qtmultimedia-plugin-audio-pulseaudio qt5-qtmultimedia-plugin-mediaservice-gstcamerabin qt5-qtmultimedia-plugin-mediaservice-gstmediaplayer qt5-qtmultimedia-plugin-resourcepolicy-resourceqt rmtfs spirv-tools-libs tqftpserv wayland wayland-egl 
   plus wildcards `ofono* pulseaudio* qt5-qtmultimedia* mesa-* libcamera* libqmi* libssc*`.
4. `ssu re 5.2.0.22; ssu ur; zypper ref`, `zypper dup --dry-run` (read it: 574 upgrades, 7 new,
   16 "downgrades" that are the same upstream versions with lower OBS release counters, no removals).
5. Detached: `systemd-run --unit=sfos-dup sh -c "zypper -n dup --download-only && zypper -n --no-refresh dup --replacefiles"`.
   rc 107 from an old `%postun` (qtquickparticles) is harmless; a second dry run says "Nothing to do".
6. Remove `/etc/ssu/ssu.ini.rpmnew`, re-arm the one-shot boot (`DATA=/mnt/nixos sfos-boot-once.sh`), reboot.

First boot: lipstick, pulseaudio, ofono 2.12.0+git4 (built on the 5.1 SDK target) and mobile data
work. `policies-setup.service` fails (usb-moded not on D-Bus; not new). No 5.2 SDK target is published
yet (releases.sailfishos.org/sdk/targets/ tops out at 5.1.0.11); builds stay on 5.1.
