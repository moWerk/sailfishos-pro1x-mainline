# SailfishOS on the F(x)tec Pro1-X with a mainline kernel (bring-up prototype)

SailfishOS 5.2.0.22 (aarch64, Jolla's repositories) running on the F(x)tec Pro1-X
(qx1050, Qualcomm SM6115) on its own mainline kernel (7.3.0-qx1050, started from the
sm6115-mainline tree at 7.1), without libhybris
or any Android pieces. The work builds on nyymi609's NixOS port of the device
(fxtec-pro1x-nixos: kernel patches, firmware set, UCM audio profile, U-Boot) and borrows its
userspace shape from the PinePhone port (sailfish-on-dontbeevil).

This is a prototype made with LLM assistance (Claude, driven and reviewed by moWerk) in
October and November 2026. It is meant to be read and rewritten by someone with porting and upstreaming
experience, not merged as is. Every decision that was taken is written down here or in
`notes/`; where something was guessed, the file says so.

## State (updated 2026-11-10)

| Area | State | Where |
|---|---|---|
| OS | SailfishOS 5.2.0.22, upgraded in place from 5.1.0.11; Jolla Store, OS update page, accounts and sync from `patterns-sailfish-consumer-generic` | `notes/sfos-5.2-upgrade.md`, `scripts/stage1-base.sh` |
| Kernel | own 7.3.0-qx1050: v7.3 + 68 patches (`kernel/patches-7.3/`, what each is and its upstream status in `kernel/HANDOVER.md`), built from `arm64 defconfig` plus `kernel/pro1x.fragment` and `kernel/sfos.fragment` by `kernel/build-kernel.sh` in a rootless podman container; the build fails if a fragment line is lost. Daily kernel since 2026-11-02 carries the series up to 0060 (RPM vmin with prompt SMS and calls), since 2026-11-08 up to 0066 plus Bluetooth LE | `kernel/` |
| Mobile data | works, toggle in Settings works: ofono patch 0020 (DPM open port, QMAPv4 with DL aggregation, bind without client type), rmnet links with MAPv4 checksum offload | `patches/ofono/`, `sparse/usr/bin/droid/qx1050-modem-prep.sh` |
| Calls, SMS | calls connect both ways, ringtone plays, SMS; Settings > SIM cards (`jolla-settings-networking-multisim`) | |
| Call audio | **works** both ways on the earpiece and the speaker, switching between them takes about 2 s, volume keys set the call volume (2026-10-29). The codec DMA back ends stall on every start in the call profile; the helper restarts them. Bluetooth earbuds in calls, wired headset and an echo canceller still open | `packages/qx1050-voicecall/README.md` |
| Sensors | **working** from boot: accelerometer, magnetometer/compass, light, proximity (gyroscope adaptor built, untested) and the step counter (the Sensor Core's pedometer: 181 steps counted for 170 walked; logged per day by sensorlogd, the AsteroidOS step logger by dodoradio, and shown in the top menu by `steps-topmenu`) through the ADSP Sensor Core (hexagonrpcd + libssc + our sensorfw adaptors). Auto-rotation, tilt in apps, auto brightness, proximity in mce. libssc patched so a missing sensor service no longer blocks sensorfwd (lipstick 25 s -> 7 s) | `packages/sensorfw-qt5-ssc/`, `packages/sensorlogd/`, `packages/steps-topmenu/`, `notes/sensors-design.md` |
| Charging | Settings > Battery charge limit: mce switches the PMI632 charge enable through a new `charging_enabled` attribute (kernel patch 0017); stops at the set limit and **holds its reading** since 0068 (2026-11-09: 80 % held at 0 mA while the voltage relaxed; before, the gauge re-anchored on the raised voltage and read 98 %). Plug-in shows the charging notification with its sound, then the USB mode notification; unplug is silent (kernel patch 0025, usb-moded cable delay 500 ms). Low battery warning works. While charging on a DCP charger the phone stays awake until the limit or termination (0040); on a host port it stays awake so ssh keeps working (0048); a host misread as DCP after a sleep gets one more detection (0052); a cable plugged into a sleeping phone wakes it within 2 s (0046, 0047). Battery estimate from the PMIC ADC (`pro1x-adc-battery`, out of tree): no OCV anchoring right after a charger (0036), the sleep OCV as anchor (0044). 5 V only, input limited to 1.5 A (no USB-PD fast charging yet; needs battery temperature reporting first) | `sparse/etc/mce/92-qx1050-charging.ini`, `sparse/var/lib/environment/usb-moded/`, kernel 0017, 0025, 0036, 0040, 0044, 0046-0048, 0052, 0068 |
| Display | panel turns off with a clean fade: the panel driver now sends display off while video still runs (kernel patch 0023); before, every blank ended in one corrupted full-brightness frame. Wake: the panel came out of sleep at its power-on brightness before the backlight was enabled (bright, dim, then the ramp); kernel patch 0030 sends the level before display on: one unclean ramp seen in about 12 wakes since, not reproducible. No brightness writes while the DSI host is off (0042: the `-22` errors at every blank are gone). The panel's enable GPIO follows the panel (0053), which had stayed high through sleep | kernel 0023, 0030, 0042, 0053 |
| Double-tap to wake | **works**: double-tap the dark screen to wake it, as on Jolla's own devices (many community ports lack it). mce's doubletap module with the touchscreen driver's wakeup gesture | `sparse/etc/mce/` |
| Sleep | **suspends** (s2idle) and reaches RPM **vmin** since 2026-11-02: PM_AUTOSLEEP (the kernel had none, so mce could never suspend), the TB-J606L fastrpc and SMP2P fixes (0032-0034, the ADSP sensor process crashed at every resume before), the cluster idle-state parameters the PSCI firmware had refused (0045: 96 % frozen instead of 50 %, no RCU stalls), the panel enable GPIO switched with the panel (0053: 73-79 to 49 mA), the q6afe LPASS vote handle (0054: the ADSP sleeps for the first time), the UFS lane clock backport and UFS power-down in system suspend (0055, 0056), dispcc on the active-only XO (0057), a system power domain under the MPM (0049-0051) and the modem edge as a wakeup source (0058-0060: SMS and calls arrive at once while asleep). Measured by the PMIC fuel gauge: 35-40 mA asleep on battery over hours, vmin about 85-95 % of the time (was about 100 mA). Wakes: power key, keyboard slide, double-tap (0035), RTC alarms, USB-C plug-in (0046, 0047), the modem. Left: the floor varies 29-32 vs 34-38 mA between boots, cause unknown; RTC alarms from dsme's iphb clients about every 6 min; the BT UART and IPA wakeups are turned off by udev rules (unmeasured); the Wi-Fi reconnect race, see Wi-Fi. Overnight 2026-11-10/11: 12 % in about 7 h asleep on the 4.5-year-old battery, Wi-Fi up, SIM in, fingerprint daemon running (author) | `notes/sleep.md` |
| Touch | works, multitouch | kernel |
| Wi-Fi | ath10k (WCN3950, 1x1). Measured against a wired host (2026-10-30): 5 GHz VHT80 about 195 Mbit/s down, 210-225 Mbit/s up at -70 to -80 dBm; 2.4 GHz HT40 about 97 down, 80 up. connman, stable MAC derived from machine-id (connman is restarted once when wlan0 appears, on purpose: `notes/boot-audit.md`). Wake-on-WLAN (disconnect, magic packet) keeps the association through suspend (2026-10-30). The AP drops the sleeping station every 10-15 min and the reconnect raced the next suspend into a WLAN firmware assert that took the modem down (5 in 7 h); kernel patches 0061 and 0062 hold the system awake for the reconnect and hand a beacon miss to mac80211: 0 of 75 deauths raced and 0 asserts in 12 h (2026-11-06) | `sparse/lib/udev/rules.d/70-qx1050-wlan-mac.rules`, `70-qx1050-wowlan.rules`, kernel 0061, 0062 |
| Storage, memory | UFS reads at queue depth 1 nearly doubled (Benchymark storage 72/48 -> 76/86 MB/s write/read): the UFS PM QoS vote now follows the clocks (kernel patch 0024), so CPUs stay out of deep idle (666 us exit latency) while UFS is busy. zram swap: 3.5 GB lz4 (Jolla's `zram` package, CONFIG_ZRAM=m) (2026-10-29) | `kernel/patches-7.3/0024-*`, `kernel/sfos.fragment` |
| GPU | Adreno 610 on freedreno (Mesa 25.1.8); GPU memory-bandwidth votes added (kernel patch 0021): +8 to +37 % on bandwidth-bound Benchymark stages | `notes/gpu-perf.md` |
| Camera | libcamera 0.7.2 with the GPU software ISP, about 32 fps at 1280x960, rear autofocus, video recording (Venus H.264). Flash: photo flash (LED pre-flash around the capture) and video light from jolla-camera (2026-10-30): the camerabin plugin drives the PMI632 flash LED through its V4L2 flash subdevice (the camera app's sandbox cannot write /sys); kernel patch 0027 links the LED to the rear camera, CONFIG_V4L2_FLASH_LED_CLASS=m. **Tap to focus, ISO and exposure compensation** in jolla-camera (2026-10-30): the software ISP focuses on the tapped area (AfWindows) and reports AfState, ISO 100-3200 is analogue gain, the flash LED lights the scan as a focus assist (libcamera patches 0012-0014, camerabin patch 0012). Photo viewfinder keeps the size the app asks for and the camera switches to the full mode only for the still (libcamera 0017, camerabin 0014; in daily use in jolla-camera on both cameras, author). Longer exposures in dark scenes (libcamera 0018, from the Guix port; the improvement confirmed by the author); the faster AGC (0019) broke focus at 50 cm and is left out. Open (2026-11-08): very close macro does not trigger AF, the tap circle turns red although the image gets sharp after 2-3 s. Test round 2026-11-11 (author): rear and front photos from the app with correct orientation; video recorded from the app UI plays fairly smoothly, but **without sound**, rear videos taken in portrait play rotated 90 degrees counterclockwise (landscape and the front camera are right), and Gallery shows rear videos black until played | `patches/libcamera/`, `patches/qtmultimedia/`, `notes/guix-port-merge.md` |
| Notification LED | mce drives the RGB LED (one multicolor LED, PMI632 LPG) through a new `multicolor` backend in mce-plugin-libhybris: calls green, messages blue, notifications cyan blinking with the screen off, battery full breathing white (2026-10-29) | `patches/mce-plugin-libhybris/`, `sparse/etc/mce/93-qx1050-notification-led.ini` |
| Torch | top-menu flashlight toggle (Jolla's `jolla-settings-system-flashlight`) with the port's `sysfs-flashlight` provider: Jolla's droidmedia-flashlight with the Android camera call replaced by the PMI632 flash LED in torch mode (2026-10-29). | `packages/sysfs-flashlight/`, `sparse/etc/sysfs-flashlight.conf` |
| Bluetooth | works: audio streaming (A2DP) to Moondrop Space Travel Ultra 2 earbuds; toggling Bluetooth off and on several times reconnects every time (the 7.1 power-down failure is gone on 7.3). **Bluetooth LE** since 2026-11-08: arm64 defconfig turns BT_LE off, so the port had no LE at all; with it Amazfish bonded two AsteroidOS watches (LE Secure Connections, numeric comparison, time sync, notifications). Link drops seen with the Pixel Watch are on the watch side (`notes/guix-port-merge.md`) | `kernel/sfos.fragment` |
| Audio | speaker and microphone confirmed, earpiece and speaker in calls (PulseAudio 17 + UCM2 + ohm policy). **No clicks or pops**: the amplifiers switch on after the codec (patches 0019, 0022) and off at the stream mute before the codec stops (patch 0026: they are codecs on the playback link, listed first), which removed the loud click at every PulseAudio idle suspend and at boot (2026-10-29). Speaker louder since 2026-11-08 (HPH Volume 20 while the speaker is on, as in the Guix port; a web stream at full volume without clipping). The sound card re-registration at boot is still needed (without it the speaker can stay silent). The faint amplifier hiss while a stream is open is inaudible since a few kernel iterations (author, 2026-11-10). Left: **the headphone jack**: with the detect switch types from the stock device tree (0067) plug edges reach MBHC, but a plug is reported only sometimes, headphones then play together with the speaker, and a device switch under a running stream stalls the RX PCM (`hw_ptr` 0; recovery `pactl suspend-sink ... 1` then `0`); in-call mute built (voicecall 0.1-8), untested. Test round 2026-11-11 (author): **wired headphones do not take over**, the speaker keeps playing and both outputs sound; Bluetooth earbuds ring and play the disconnect sound, but call audio and the mic stay on the phone | `sparse/etc/pulse/`, `sparse/usr/share/alsa/ucm2/`, `kernel/patches-7.3/0026-*`, `notes/guix-port-merge.md` |
| Settings | hardware feature list (`hw-settings.ini`) declared, which gates sensors, the battery indicator and several Settings items; orientation lock, rotating home screen, sideloading pages | `sparse/usr/share/csd/settings.d/hw-settings.ini` |
| SD card | detected, shown in Settings > Storage | |
| USB | usb-moded on configfs: plug notification, charging only, developer mode (192.168.2.15), MTP file transfer. Unplugged, the DWC3 controller runtime-suspends and its PHY supplies switch off (udev rule, `power/control` auto at bind; Android's init does the same): clean sleeps 29-32 mA instead of 40-42 on the boots where it takes, not on every boot (the 5 s autosuspend timer never elapses when wakes are shorter than that; open) | `sparse/etc/usb-moded/`, `sparse/usr/bin/droid/qx1050-usb-gadget.sh`, `sparse/lib/udev/rules.d/70-qx1050-usb-runtime-pm.rules` |
| Boot | straight from the Android bootloader: SailfishOS boot image in `boot_a`, no second bootloader (2026-10-29); the slot is marked successful after every boot (qbootctl). User session at 14 s, lipstick ready at about 22 s. Dual boot with NixOS through U-Boot stays possible | `notes/boot.md`, `scripts/build-bootimg.sh` |
| Alarm | **power-off alarm works**: the phone switches itself on at a Clock alarm and rings (dsme programs the PMIC RTC relative to its own clock; Linux cannot set that RTC, TrustZone) | |
| Modem | the intermittent watchdog crash shortly after boot ("DOG detects stalled initialization", 12 of 25 boots) was a 7.3 qrtr HELLO race that left the modem without the rmtfs announcement; fixed by kernel patch 0029 (from the Lenovo TB-J606L port): 0 crashes in the boots since 2026-10-30. Incoming calls: since the modem gets its MCFG it selects the SIM's VoLTE carrier profile (voice domain ps-preferred) and the network routed incoming calls to VoLTE, which the port has no IMS stack for; the modem preparation sets cs-preferred at every modem start, and ofono patch 0021 registers for call notifications: incoming calls ring awake and suspended (2026-10-30). The modem wakes the phone from sleep for SMS and calls (0058-0060). **Open: any modem crash (a subsystem restart) resets the phone into 900e crash-dump mode a second after the recovered modem is up**, with nothing in the log, unless Wi-Fi was power-cycled once after boot; `qx1050-wifi-cycle.service` does that (connman Wi-Fi off and on) and every recovery since completes. Root cause unknown; a PMIC L8 lead was inconclusive and the condition needs a SIM | `notes/modem-dog-crash.md`, `notes/modem-recovery-crash.md`, `sparse/usr/bin/droid/qx1050-wifi-cycle.sh` |
| Vibration | ngfd ffmemless, AW86224 at gain 7 / drive 100 (`tools/haptics-tune.sh` sweeps both) | `sparse/usr/share/ngfd/plugins.d/60-qx1050-ffmemless.ini`, `sparse/etc/modprobe.d/aw86224.conf` |
| Volume keys | work (lipstick + PulseAudio mainvolume), also for the call volume | |
| Camera button | **hold it to open the camera** while the phone is unlocked (2026-11-11, author): one gpio-key, KEY_CAMERA, no focus stage. Qt's evdev keymap has no KEY_CAMERA, so lipstick-jolla-home's own shutter handler never sees it; a user service makes the same `com.jolla.camera.ui.showViewfinder` call after a 0.6 s hold, only with the lock screen down and the display on, never from the lock screen (against pocket presses). A press also wakes the phone to the lock screen, **from deep sleep too** (author, off USB after a minute asleep), although the DT key carries no `wakeup-source`; why it wakes is not traced | `sparse/usr/bin/droid/qx1050-camera-key.py`, `sparse/usr/lib/systemd/user/qx1050-camera-key.service` |
| Hardware keyboard | German QWERTZ, Fn legends on AltGr (level 3). Rotation with the keyboard open follows the accelerometer only; a landscape lock while the slide is open is wanted | `sparse/usr/share/X11/xkb/symbols/fxtec_vndr/pro1x`, DT patch |
| Keyboard slide, backlight | mce drives both; DT reports the slide the way mce reads it. The backlight follows the slide and the display (2026-10-30): `sfos-dt` had inverted the LED with the slide switch, which kept it lit with the keyboard closed (kernel patch 0028) | `sparse/etc/mce/`, kernel patches `sfos-dt`, 0028 |
| GNSS | **fix under open sky** since 2026-11-06 (3-5 m, 1 Hz through a 2.5 h drive at up to 140 km/h). The modem gets its MCFG files (`qx1050-modem-pr.service`; without them it heard no satellite) and keeps its EFS across boots (rmtfs on files in /var/lib/rmtfs, seeded once from the partitions, which stay read-only). Position reaches SailfishOS through `geoclue-provider-hybris-qmi`: Jolla's geoclue-hybris with a QMI LOC backend over QRTR in place of the Android HAL (session start, stop, idle exit, satellites, time and position injection). Assistance: the GNSS engine comes up locked for injected data; the modem preparation unlocks it, and the provider downloads and injects XTRA orbit data when less than a day is left. Cold start on the patio: about 13 s to first fix with XTRA, about 4 min with time only (2026-11-08). Open: nothing keeps the system awake during a session; Pure Maps updated about once a minute while the modem fixed at 1 Hz (app or velocity path, not the provider) | `notes/gnss.md`, `packages/geoclue-provider-hybris-qmi/`, `scripts/qmi-loc-*.py` |
| Lock code, encryption | **lock code works** (asked at the first boot, then the lock screen as on Jolla's devices; fingerprint unlock beside it since 2026-11-11). Encryption not yet: home is a directory, not its own volume; the SailfishOS LVM layout (next) gives home encryption. Plan and draft tools (2026-10-30): LVM initramfs, flashable image set, kernel update RPM | `notes/lvm-encryption-plan.md`, `notes/flashable-images.md` |
| Fingerprint | **works: enrol from Settings, unlock on the lock screen** (2026-11-11, kernel kfp2; two fingers enrolled by the author, both unlock after a sleep). The MicroArray sensor hangs off a TrustZone-owned SPI bus and is driven by the stock `mafp` trusted app through Bartosz Wróbel's out-of-tree qseecomtee driver and qsee-supplicant; `sailfish-fpd-mafp` (this repo) implements `org.sailfishos.fingerprint1` on that protocol, Jolla's `sailfish-devicelock-fpd` gives Settings its Fingerprint section. The three hard hangs of 2026-11-09 were two stacked kernel bugs (a listener round cap that counted served requests, and mainline tzmem passing a positive firmware status through `ERR_PTR()`), fixed in `kernel/patches-fp/`. Open: the match rate (about one touch in three matched in the first tests), the sensor interrupt (the daemon polls), sensor power stays on, one dead TA instance so far (recovered by reloading it), and the fingerprint kernel is still separate from the daily one. Needs the stock `mafp.mbn` (not redistributable). Test round 2026-11-11 after a reboot: 20 of 20 lock screen unlocks (author) | `notes/fingerprint.md`, `packages/sailfish-fpd-mafp/`, `packages/qsee-supplicant/`, `kernel/patches-fp/` |
| Performance | Benchymark: CPU about as hybris; storage 76/86 MB/s write/read (was 72/48); GPU +8 to +37 % with the bandwidth votes; zram swap | `notes/gpu-perf.md` |
| Not done yet | packaging: the mainline HADK's five-repository layout and build scheme, then OBS builds and OTA updates (the kernel update RPM is a draft, firmware comes from the Nix package); the fingerprint kernel fixes in the daily kernel; kernel 7.4. Modem: VoLTE/IMS, a second SIM, IPv6 and roaming untested. Audio: headphone switching, Bluetooth audio in calls, wired headset (none at hand to test), echo canceller. Camera: sound in videos, portrait orientation of rear videos. USB: the gadget dies after the first suspends (`HS-PHY not in L2`, a reboot restores it), and after a replug the phone can come up in charging-only mode; Wi-Fi answers only while the screen is on. Untested: gyroscope. No USB-PD fast charging | |
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
- `packages/`: RPM sources built for the port: qrtr, rmtfs, tqftpserv (linux-msm), protobuf-c, libssc, hexagonrpc and `sensorfw-qt5-ssc` (sensors), `sensorlogd` and `steps-topmenu` (steps), `qx1050-voicecall` (call audio), `libcamera-qx1050`, `sysfs-flashlight` (torch), `geoclue-provider-hybris-qmi` (GNSS), `qbootctl` (A/B slot), `kernel-qx1050` (kernel update package, draft). Each has a README with its tested state.
- `tools/`: `ffrumble.c` (force-feedback test), `audio-loopback-test.sh` (mic-vs-speaker
  oracle, lets audio bring-up run without a listener), `haptics-tune.sh`, `qgsleep/` (a kernel
  module that reads the PMIC fuel gauge's sleep samples: the sleep-current meter behind every
  figure in `notes/sleep.md`).
- `notes/`: design and investigation notes (`boot.md`, `sleep.md`, `gnss.md`, `sensors-design.md`,
  `modem-dog-crash.md`, `modem-recovery-crash.md`, `guix-port-merge.md`, `lvm-encryption-plan.md`,
  `waydroid.md`, `upstream-glink-wakeup.md`), kernel config, mer-kernel-check output, stage logs,
  `research/` (reports on what existing mainline ports already solved), `upstream.md` (items
  worth sending upstream).

The kernel is an own tree since 2026-10-26: a plain v7.3 source tree plus the series in
`kernel/patches-7.3/` (68 patches; origin, status and device test of each in
`kernel/HANDOVER.md`), configured from `arm64 defconfig` plus `kernel/pro1x.fragment` (the
board) and `kernel/sfos.fragment` (what SailfishOS needs: cgroup v1, quota netlink, netfilter
legacy, IPsec, PPP/L2TP, Bluetooth profiles and LE, PM_AUTOSLEEP), built by
`kernel/build-kernel.sh` in a rootless podman container with the Debian cross toolchain. The
build fails if a fragment line does not survive Kconfig. The NixOS flake is no longer used.

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
| qrtr, rmtfs, tqftpserv | built here from linux-msm | modem EFS (files in /var/lib/rmtfs) and MCFG |

Missing from the Jolla stack for a mainline Qualcomm phone: oFono with QRTR and the slot
manager (sailfishos-native carries 2.13 + the Jolla patches, nobody wired `qrtrqmi` into the
slot manager yet: ~270 lines, the gobi slot shim is the template), a sensorfw adaptor for
the ADSP sensors, jolla-camera on libcamerasrc.

## How a session works

The phone sits on USB in developer mode (192.168.2.15) or on Wi-Fi. Kernel builds take about
20 min on the e15 (`~/sailfish-build`, see `fetch-e15-kernel.sh`). A kernel test cycle: build
the boot image (`scripts/build-bootimg.sh`), keep the previous one, write it to `boot_a` (from
SailfishOS with `dd`, or `fastboot flash boot_a` after `systemctl reboot bootloader`), swap the
modules, reboot, then check ssh, Wi-Fi, the modem and the sound card before anything else; an
unattended run has a health gate and a rollback to the previous image. Sleep current is read
from the fuel gauge with `tools/qgsleep` over windows with USB power off. Every fallback image
and module set is kept on the phone under `/root` with a `pre-<kernel>` name.

## Credits

nyymi609 (NixOS port, kernel patches, UCM, haptics driver, the voice-call helper), Adam Pigg /
piggz (PinePhone port and the native branch of droid-hal-configs, the PulseAudio build),
Mister_Magister / Michał (freedreno config via the Moto X 2014 port, the Pro1 German layout
push, the mainline HADK), Kabouik / mlaparie (hybris Pro1-X mce configs; the Guix port of the
Pro1-X, from which Bluetooth LE, the GNSS engine unlock, the louder speaker, the modem wake
filter and the BT UART and IPA wake hygiene were taken), TheKit/NotKit (xkeyboard-config-pro1),
Dylan Van Assche (libssc, the Sensor Core library every sensor here goes through), Arseniy
Movshev / dodoradio (asteroid-sensorlogd, the step logger `packages/sensorlogd` ports, with its
QML module), haltia-de (the Lenovo TB-J606L port: the qrtr HELLO fix 0029, the fastrpc and
SMP2P sleep fixes 0032-0034, the psci late init 0038), Stephan Gerhold (q6voice), Milan Zamazal
(software-ISP autofocus for libcamera), Konrad Dybcio (the sm6115 cluster idle-state fix of 2023
that 0045 repeats), Bartosz Wróbel (the qseecomtee driver and qsee-supplicant behind the
fingerprint support), the sailfish-fpd-community contributors (the D-Bus side of the fingerprint daemon), the sailfishos-native and sailfish-on-dontbeevil contributors,
moWerk (author, device owner, every ground-truth test, the boot splash artwork). The splash background is a free stock photo by Archie Pryke on Pexels.

See `notes/upstream.md` for what should go back to the projects above, and the port log
(`~/sailfish/reports/pro1x-mainline-port-log.md` on the build host) for the day-by-day record.
