# SailfishOS on the F(x)tec Pro1-X with a mainline kernel (bring-up prototype)

SailfishOS 5.2.0.22 (aarch64, Jolla's repositories) running on the F(x)tec Pro1-X
(qx1050, Qualcomm SM6115) on its own mainline kernel (7.3.0-qx1050, started from the
sm6115-mainline tree at 7.1), without libhybris
or any Android pieces. The work builds on nyymi609's NixOS port of the device
(fxtec-pro1x-nixos: kernel patches, firmware set, UCM audio profile, U-Boot) and borrows its
userspace shape from the PinePhone port (sailfish-on-dontbeevil).

This is a prototype made with LLM assistance (Claude, driven and reviewed by moWerk) in
October 2026. It is meant to be read and rewritten by someone with porting and upstreaming
experience, not merged as is. Every decision that was taken is written down here or in
`notes/`; where something was guessed, the file says so.

## State (updated 2026-10-30 09:10 CET)

| Area | State | Where |
|---|---|---|
| OS | SailfishOS 5.2.0.22, upgraded in place from 5.1.0.11; Jolla Store, OS update page, accounts and sync from `patterns-sailfish-consumer-generic` | `notes/sfos-5.2-upgrade.md`, `scripts/stage1-base.sh` |
| Kernel | own 7.3.0-qx1050: v7.3 + 31 patches (`kernel/patches-7.3/`, what each is and its upstream status in `kernel/HANDOVER.md`) | `kernel/` |
| Mobile data | works, toggle in Settings works: ofono patch 0020 (DPM open port, QMAPv4 with DL aggregation, bind without client type), rmnet links with MAPv4 checksum offload | `patches/ofono/`, `sparse/usr/bin/droid/qx1050-modem-prep.sh` |
| Calls, SMS | calls connect both ways, ringtone plays, SMS; Settings > SIM cards (`jolla-settings-networking-multisim`) | |
| Call audio | **works** both ways on the earpiece and the speaker, switching between them takes about 2 s, volume keys set the call volume (2026-10-29). The codec DMA back ends stall on every start in the call profile; the helper restarts them. Bluetooth earbuds in calls, wired headset and an echo canceller still open | `packages/qx1050-voicecall/README.md` |
| Sensors | **working** from boot: accelerometer, magnetometer/compass, light, proximity (gyroscope adaptor built, untested) through the ADSP Sensor Core (hexagonrpcd + libssc + our sensorfw adaptors). Auto-rotation, tilt in apps, auto brightness, proximity in mce. libssc patched so a missing sensor service no longer blocks sensorfwd (lipstick 25 s -> 7 s) | `packages/sensorfw-qt5-ssc/`, `notes/sensors-design.md` |
| Charging | Settings > Battery charge limit: mce switches the PMI632 charge enable through a new `charging_enabled` attribute (kernel patch 0017); stops at the set limit. Plug-in shows the charging notification with its sound, then the USB mode notification; unplug is silent (kernel patch 0025: plug changes reported at once, battery event before the charger's on unplug; usb-moded cable connection delay 500 ms) (2026-10-29). Low battery warning works. 5 V only, input limited to 1.5 A (no USB-PD fast charging yet) | `sparse/etc/mce/92-qx1050-charging.ini`, `sparse/var/lib/environment/usb-moded/` |
| Display | panel turns off with a clean fade: the panel driver now sends display off while video still runs (kernel patch 0023); before, every blank ended in one corrupted full-brightness frame. Wake: the panel came out of sleep at its power-on brightness before the backlight was enabled (bright, dim, then the ramp); kernel patch 0030 sends the level before display on: one unclean ramp seen in about 12 wakes since, not reproducible | |
| Double-tap to wake | **works**: double-tap the dark screen to wake it, as on Jolla's own devices (many community ports lack it). mce's doubletap module with the touchscreen driver's wakeup gesture | `sparse/etc/mce/` |
| Touch | works, multitouch | kernel |
| Wi-Fi | ath10k (WCN3950, 1x1). Measured against a wired host (2026-10-30): 5 GHz VHT80 about 195 Mbit/s down, 210-225 Mbit/s up at -70 to -80 dBm; 2.4 GHz HT40 about 97 down, 80 up. connman, stable MAC derived from machine-id (connman is restarted once when wlan0 appears, on purpose: `notes/boot-audit.md`) | `sparse/lib/udev/rules.d/70-qx1050-wlan-mac.rules` |
| Storage, memory | UFS reads at queue depth 1 nearly doubled (Benchymark storage 72/48 -> 76/86 MB/s write/read): the UFS PM QoS vote now follows the clocks (kernel patch 0024), so CPUs stay out of deep idle (666 us exit latency) while UFS is busy. zram swap: 3.5 GB lz4 (Jolla's `zram` package, CONFIG_ZRAM=m) (2026-10-29) | `kernel/patches-7.3/0024-*`, `kernel/sfos.fragment` |
| GPU | Adreno 610 on freedreno (Mesa 25.1.8); GPU memory-bandwidth votes added (kernel patch 0021): +8 to +37 % on bandwidth-bound Benchymark stages | `notes/gpu-perf.md` |
| Camera | libcamera 0.7.2 with the GPU software ISP, about 32 fps at 1280x960, rear autofocus, video recording (Venus H.264). Flash: photo flash (LED pre-flash around the capture) and video light from jolla-camera (2026-10-30): the camerabin plugin drives the PMI632 flash LED through its V4L2 flash subdevice (the camera app's sandbox cannot write /sys); kernel patch 0027 links the LED to the rear camera, CONFIG_V4L2_FLASH_LED_CLASS=m. **Tap to focus, ISO and exposure compensation** in jolla-camera (2026-10-30): the software ISP focuses on the tapped area (AfWindows) and reports AfState, ISO 100-3200 is analogue gain, the flash LED lights the scan as a focus assist (libcamera patches 0012-0014, camerabin patch 0012). Photo viewfinder (2026-10-30, headless test only, not yet seen in jolla-camera): it now keeps the size the app asks for (1920x1080, the sensor's 4x4 binned 1984x1080 mode, ~54 fps instead of ~10 at 3200x1800) and the camera switches to the 4000x3000 mode only for the still, done 0.95 s after the shutter; exposure, colour and focus carry over the mode change (libcamera 0017, camerabin 0014; `QT_GSTREAMER_LIBCAMERA_FULL_VIEWFINDER=1` restores the old behaviour). Untested: focus held after a tap and no rescan on a sharper frame (libcamera 0015, camerabin 0013), the GPU debayer's centred crop and statistics window (libcamera 0016) | `patches/libcamera/`, `patches/qtmultimedia/` |
| Notification LED | mce drives the RGB LED (one multicolor LED, PMI632 LPG) through a new `multicolor` backend in mce-plugin-libhybris: calls green, messages blue, notifications cyan blinking with the screen off, battery full breathing white (2026-10-29) | `patches/mce-plugin-libhybris/`, `sparse/etc/mce/93-qx1050-notification-led.ini` |
| Torch | top-menu flashlight toggle (Jolla's `jolla-settings-system-flashlight`) with the port's `sysfs-flashlight` provider: Jolla's droidmedia-flashlight with the Android camera call replaced by the PMI632 flash LED in torch mode (2026-10-29). | `packages/sysfs-flashlight/`, `sparse/etc/sysfs-flashlight.conf` |
| Bluetooth | works: audio streaming (A2DP) to Moondrop Space Travel Ultra 2 earbuds; toggling Bluetooth off and on several times reconnects every time (the 7.1 power-down failure is gone on 7.3) | |
| Audio | speaker and microphone confirmed, earpiece and speaker in calls (PulseAudio 17 + UCM2 + ohm policy). **No clicks or pops**: the amplifiers switch on after the codec (patches 0019, 0022) and off at the stream mute before the codec stops (patch 0026: they are codecs on the playback link, listed first), which removed the loud click at every PulseAudio idle suspend and at boot (2026-10-29). The sound card re-registration at boot is still needed (without it the speaker can stay silent). Left: a faint amplifier hiss while a stream is open | `sparse/etc/pulse/`, `sparse/usr/share/alsa/ucm2/`, `kernel/patches-7.3/0026-*` |
| Settings | hardware feature list (`hw-settings.ini`) declared, which gates sensors, the battery indicator and several Settings items; orientation lock, rotating home screen, sideloading pages | `sparse/usr/share/csd/settings.d/hw-settings.ini` |
| SD card | detected, shown in Settings > Storage | |
| USB | usb-moded on configfs: plug notification, charging only, developer mode (192.168.2.15), MTP file transfer | `sparse/etc/usb-moded/`, `sparse/usr/bin/droid/qx1050-usb-gadget.sh` |
| Boot | straight from the Android bootloader: SailfishOS boot image in `boot_a`, no second bootloader (2026-10-29); the slot is marked successful after every boot (qbootctl). User session at 14 s, lipstick ready at about 22 s. Dual boot with NixOS through U-Boot stays possible | `notes/boot.md`, `scripts/build-bootimg.sh` |
| Alarm | **power-off alarm works**: the phone switches itself on at a Clock alarm and rings (dsme programs the PMIC RTC relative to its own clock; Linux cannot set that RTC, TrustZone) | |
| Modem | intermittent watchdog crash shortly after boot ("DOG detects stalled initialization"; 12 of 25 boots on 2026-10-29/30, up to three in a row, 40 s each): Wi-Fi, data and SIM then come up about a minute late. Cause found: a 7.3 qrtr HELLO race leaves the modem without the rmtfs announcement; fixed by kernel patch 0029 (from the Lenovo TB-J606L port): 0 of 8 boots crashed (2026-10-30); a day-long run is still to show whether the later crashes seen on long sessions are gone too | `notes/modem-dog-crash.md` |
| Vibration | ngfd ffmemless, AW86224 at gain 7 / drive 100 | `sparse/usr/share/ngfd/plugins.d/60-qx1050-ffmemless.ini`, `sparse/etc/modprobe.d/aw86224.conf` |
| Volume keys | work (lipstick + PulseAudio mainvolume), also for the call volume | |
| Hardware keyboard | German QWERTZ, Fn legends on AltGr (level 3) | `sparse/usr/share/X11/xkb/symbols/fxtec_vndr/pro1x`, DT patch |
| Keyboard slide, backlight | mce drives both; DT reports the slide the way mce reads it. The backlight follows the slide and the display (2026-10-30): `sfos-dt` had inverted the LED with the slide switch, which kept it lit with the keyboard closed (kernel patch 0028) | `sparse/etc/mce/`, kernel patches `sfos-dt`, 0028 |
| GNSS | path proven, no fix yet (2026-10-30): the modem's QMI location service streams NMEA, positions and satellite info once its NMEA types are enabled (`scripts/qmi-loc-probe.py`); indoors no satellites. Nothing feeds geoclue yet (plan: gpsd); AGPS (XTRA, time and position injection) planned | `notes/gnss.md` |
| Lock code, encryption | not yet: home is a directory, not its own volume; the SailfishOS LVM layout (next) gives home encryption. Plan and draft tools (2026-10-30): LVM initramfs, flashable image set, kernel update RPM | `notes/lvm-encryption-plan.md`, `notes/flashable-images.md` |
| Performance | Benchymark: CPU about as hybris; storage 76/86 MB/s write/read (was 72/48); GPU +8 to +37 % with the bandwidth votes; zram swap | `notes/gpu-perf.md` |
| Still tied to NixOS | the rootfs is a directory on the NixOS-formatted userdata partition (next: SailfishOS LVM layout, which home encryption needs), firmware set from the Nix package | `notes/boot.md` |

## Custom boot splash

On the Pro¹-X the first logo after power-on (Android) is drawn by the signed firmware and
cannot be changed. Its `splash` partition, which on the Pro¹ held the boot logo, is ignored:
tested with the Pro¹'s format (`DDPH` header) and with Qualcomm's UEFI format (16 KiB of
zeros, then a BMP). Since this port boots its own kernel and initramfs, the screen after the
Android logo is ours: instead of the scrolling kernel console the phone shows a full-screen
image until the SailfishOS UI takes over.

1. Draw a 1080x2160 image (portrait; for a landscape design rotate it 90° before export).
   The default is `initramfs/splash/horizon-sailfish-portrait-memeline-73.png` (GIMP source:
   the `.xcf` next to it).
2. Build the initramfs: `scripts/build-initramfs.sh busybox` takes the default image,
   `SPLASH=my.png` another one (PNG, BMP or PPM; converted with ImageMagick), `SPLASH=`
   none.
   `initramfs/init` draws it on `/dev/fb0` with busybox `fbsplash` as its first step.
3. Build the boot image with `scripts/build-bootimg.sh`: its default command line keeps the
   console off the screen (`console=ttyMSM0` only, `logo.nologo`,
   `vt.global_cursor_default=0`), so nothing draws over the image.
4. Flash `boot_a` (`fastboot flash boot_a`, or `dd` to `/dev/disk/by-partlabel/boot_a` from
   SailfishOS).

A few frames of glitches remain between the Android logo and the splash, while the display
driver takes over. Details in `notes/boot.md`.

Getting it running is not yet a download-and-flash affair: the rootfs is built with
`scripts/stage1-base.sh` and `stage2-device.sh`, the kernel with `kernel/build-kernel.sh`,
and its rootfs still lives on the partition of nyymi609's NixOS port. Read `notes/` before trying.

## Layout

- `scripts/repos.sh`, `stage1-base.sh`, `stage2-device.sh`: build the rootfs tarball with
  zypper in an aarch64 podman image imported from the Sailfish SDK target. Stage 2 does what
  a kickstart `%post` does (rpm platform, rebuilddb, oneshot, ssu), installs kernel modules
  and firmware, applies `droid-config-qx1050/sparse`, runs `kickstart/post/*`.
- `scripts/install-on-nixos.sh`: unpacks the tarball into `/sfos/rootfs` on the NixOS ext4,
  resets file capabilities, installs the initramfs.
- `scripts/sfos-boot-once.sh`: arms one SailfishOS boot (U-Boot extlinux); the initramfs
  restores NixOS's `extlinux.conf`. Works from NixOS and, with `DATA=/mnt/nixos`, from SFOS.
- `scripts/sfos-kernel-install.sh`, `fetch-e15-kernel.sh`, `build-initramfs.sh`: bring-up helpers.
- `droid-config-qx1050/`: what will become the `droid-config-qx1050` package (sparse tree,
  kickstart post scripts). `sparse/etc/hw-release`, `usr/lib/hw-release` and the ssu board
  mapping are bring-up copies of files that `droid-hal-version` and `droid-configs.inc`
  generate; they must go when this is packaged.
- `packages/`: qrtr, rmtfs, tqftpserv RPM sources (linux-msm), built in the same image.
- `tools/`: `ffrumble.c` (force-feedback test), `audio-loopback-test.sh` (mic-vs-speaker
  oracle, lets audio bring-up run without a listener).
- `notes/`: kernel config, mer-kernel-check output, stage logs, `research/` (six reports on
  what existing mainline ports already solved), `upstream.md` (items worth sending upstream).

The kernel is built by the NixOS flake (`../fxtec-pro1x-nixos`, branch `mo-qwertz`):
`nix build .#kernel-sfos` = nyymi609's config + `device/kernel/sfos.fragment` (cgroup v1,
quota netlink, netfilter legacy, IPsec, PPP/L2TP, BT RFCOMM/BNEP; generated by
`scripts/gen-sfos-kernel-config.sh` against the patched tree, mer-kernel-check 29 → 12 errors)
+ `device/kernel/patches/sfos-dt.patch` (SailfishOS-only device tree changes).

## Things that look odd on purpose

- **Sound card re-probe at boot** (`qx1050-audio-reprobe.service`): on some kernel builds the
  first instance of the sm4250 card streams silence although the DSP streams and codec and
  amplifiers are powered. Re-creating the card after one PCM stream has run on the first
  instance makes it play (the service plays one second of digital silence, then reloads
  `snd_soc_sm8250`). A config bisect pointed at unrelated options, i.e. ordering decides. The
  kernel-side race is open. `qx1050-audio-selftest.service` (a bring-up unit that logs
  PLAYS/SILENT) is no longer enabled; start it by hand.
- **Slide switch inverted in the DT** (`sfos-dt.patch`): mce reads `SW_KEYPAD_SLIDE` as 1 =
  closed (N900 convention) and has no option to invert; the kernel's hall switch follows
  `input-event-codes.h` (1 = slid out). The inversion lives only in the SailfishOS kernel.
- **Fn keys as KEY_RIGHTALT**: SailfishOS's Qt 5.6 evdev keyboard handler drops scancodes
  outside its keymap, so KEY_FN never reaches a client; the hybris Pro1 port used AltGr too.
- **piggz's PulseAudio** (`pulseaudio-17.0+git2+master.20260702` from
  `nemo:devel:hw:pine:dontbeevil`): Jolla's 17.0+git1 aborts in `pa_alsa_ucm_set_port()`
  when two UCM verbs declare a device of the same name (fixed upstream 2024-01).
- **Template copies in `sparse/`**: `etc/ohm/*`, `etc/dbus-1/system.d/ohm-policy.conf`,
  `etc/pulse/x-maemo-*.table`, `var/lib/nemo-pulseaudio-parameters` come from
  mer-hybris/droid-hal-configs; a packaged droid-config gets them through its
  `droid-configs-device` submodule.
- **tmpfiles mask** (`etc/tmpfiles.d/sailfishos-cgroups.conf`): kernels without cgroup v1
  make `systemd-tmpfiles-setup` fail and with it the user session; kept for such kernels.
- **Everything copied to the device must be root-owned** (`tar --no-same-owner`,
  `cp --no-preserve=ownership` in the scripts): firejail's `fcopy` aborts on a file whose owner
  has no passwd entry, and sailjail copies `/etc/pulse` into every sandbox, so one file with a
  build-host uid there makes every sandboxed app (Camera, Browser) fail to start with only
  "application specific booster is not available" in the journal.

## Packages taken from outside Jolla's repositories

| Package | From | Why |
|---|---|---|
| pulseaudio 17.0+git2+master.20260702 | nemo:devel:hw:pine:dontbeevil | UCM assertion fix |
| ofono 2.12.0+git1, ell 0.70 | nemo:devel:hw:pine:dontbeevil | QRTR modem driver (qrtrqmi) |
| libqmi, libqmi-utils, libqrtr-glib | nemo:devel:hw:native-common | qmicli for the UIM provisioning session |
| libcamera, libcamera-tools, libevent | nemo:devel:hw:native-common | camera (libcamerasrc) |
| mesa (freedreno), qt5-qtwayland, libcamera… | nemo:devel:hw:native-common | already in stage 1 |
| qrtr, rmtfs, tqftpserv | built here from linux-msm | modem firmware storage |

Missing from the Jolla stack for a mainline Qualcomm phone: oFono with QRTR and the slot
manager (sailfishos-native carries 2.13 + the Jolla patches, nobody wired `qrtrqmi` into the
slot manager yet: ~270 lines, the gobi slot shim is the template), a sensorfw adaptor for
the ADSP sensors, jolla-camera on libcamerasrc.

## How a session works

The phone sits on Wi-Fi or USB (172.16.42.1). Kernel builds take ~20 min on the e15
(`~/sailfish-build`, see `fetch-e15-kernel.sh`) or ~1 h on the w541. A kernel test cycle from
SailfishOS itself: `sfos-kernel-install.sh build/<kernel>` (copies Image + dtb to
`/mnt/nixos/boot/sfos`, swaps `/lib/modules/7.1.0`, arms the one-shot boot), `systemctl reboot`,
wait, read `journalctl -u qx1050-audio-selftest`. The NixOS side has a random Wi-Fi MAC per
connection; find it by ssh host key.

## Credits

nyymi609 (NixOS port, kernel patches, UCM, haptics driver), Adam Pigg / piggz (PinePhone port
and the native branch of droid-hal-configs, the PulseAudio build), Mister_Magister / Michał
(freedreno config via the Moto X 2014 port, the Pro1 German layout push), Kabouik (hybris
Pro1-X mce configs), TheKit/NotKit (xkeyboard-config-pro1), the sailfishos-native and
sailfish-on-dontbeevil contributors, moWerk (author, device owner, every ground-truth test, the
boot splash artwork). The splash background is a free stock photo by Archie Pryke on Pexels.

See `notes/upstream.md` for what should go back to the projects above, and the port log
(`~/sailfish/reports/pro1x-mainline-port-log.md` on the build host) for the day-by-day record.
