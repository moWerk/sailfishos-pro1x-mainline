# Mainline SailfishOS port conventions vs. the Pro1-X prototype (research, 2026-10-23)

Read-only research. Raw copies of every fetched file are in `scratchpad/raw/`. Labels: [C] confirmed from the
named file/URL, [I] inferred, [NF] searched, not found.

## A. Confirmed

1. **Only one maintained mainline family exists: sailfish-on-dontbeevil (piggz).** PinePhone/PineTab/PinePhone Pro/
   PineTab2. Packages on OBS `nemo:devel:hw:pine:dontbeevil` (shared: kernel-adaptation-pine64/-pinephonepro, ofono,
   eg25-manager, mce, pulseaudio, sailfish-device-encryption-community...) and per device `...:dontbeevil:pinephone`
   (droid-config-pinephone + its -bluez5/-flashing/-policy-settings/-preinit-plugin/-pulseaudio-settings subpackages,
   droid-hal-version-pinephone, the three patterns). Generic native bits in `nemo:devel:hw:native-common` (mesa,
   qt5-qtwayland-wayland_egl, qtscenegraph-adaptation, plymouth-lite, libqrtr-glib, libqmi, libcamera, v4l-utils,
   gstreamer1.0-plugins-good, mce-plugin-libhybris...). [C: repo.sailfishos.org listings, raw/*]. GitHub org
   `sailfishos-native` only holds the sources of native-common packages (mesa, ofono, gpsd, alsa-ucm-conf...). [C]
2. **droid-config-<device> is the same package as on hybris**, built from `mer-hybris/droid-hal-configs`
   `droid-configs.inc` (submodule `droid-configs-device/`), which already has `native_build`: it drops
   `BuildRequires: pkgconfig(android-headers)`, `Requires: droid-hal`, and swaps `pulseaudio-modules-droid` for
   `pulseaudio-module-keepalive` (lines 84-92, 182-186). Pinephone uses piggz's `native` branch, which additionally
   guards the `ssu-kickstarts`/`kickstart-configuration` subpackages and the `%gen_ks` call (lines 114, 152, 600).
   Victara (Michał, 2026-10-14) uses mer-hybris master with `native_build 1` + `disable_tools 1`. [C: raw inc files,
   ref/droid-config-pinephone/rpm, ref/droid-config-victara/rpm]
3. **Expected repo files**: `rpm/droid-config-<dev>.spec` (macros: device, vendor, *_pretty, community_adaptation 1,
   pixel_ratio, native_build 1, optional rpm_device/rpm_vendor, ofono_enable/disable_plugins, additional_post_scripts),
   `.gitmodules`, `sparse/`, `patterns/patterns-sailfish-device-{adaptation,configuration}-<dev>.inc`,
   `delete_file.list`, optional `kickstart/`, `.portslistinfo` (victara, nabu: AUTHOR/EMAIL/BROKEN/FEATURES/DEVICE/URL).
   The inc GENERATES `/usr/share/ssu/board-mappings.d/05-<vendor>-<device>.ini` from `ssu/board-mappings.d/05-sfe.ini.in`
   and, for community_adaptation, `features.d/adaptation-community.ini` pointing at
   `nemo:/<flavour>:/hw:/<VENDOR>:/<DEVICE>/sailfishos_<maj.min>/`. [C: inc lines 466-499; community-adaptation.ini.in]
4. **hw-release comes from droid-hal-version-<device>**, never from sparse. Pinephone's 11-line spec (`native_build 1`,
   have_led, have_ffmemless) includes the **sailfish-on-dontbeevil fork** of `droid-hal-version.inc`, which comments
   out `BuildRequires: droid-hal-kernel / droid-hal-img-boot / droid-hal-img-recovery / droid-config-preinit-plugins`,
   adds a `have_ffmemless` -> `qt5-feedback-haptics-ffmemless` branch and writes only `/etc/hw-release` (`%config`).
   mer-hybris master instead BuildRequires `droid-hal-img-boot`+`-recovery` on OBS (`droid-hal-kernel` locally) even
   with `native_build`, and writes `/usr/lib/hw-release` + symlink. [C: raw/dontbeevil-dhv.inc diff]
5. **What replaces droid-hal-<device>: `kernel-adaptation-<device>`** (spec of ~15 macro lines +
   `%include kernel-adaptation-simplified/kernel-adaptation-simplified.inc`; kernel is a git submodule `linux/`, plus
   defconfig in the repo, optional `patches/` + `apply_patches 1`, `devicetrees`, `crossbuild aarch64`). The patterns
   `Requires: kernel-adaptation-<dev>` (pinephone: `kernel-adaptation-pine64`). [C: raw pine64/pinephonepro/nabu/
   halium-qx1050 specs; ref/kernel-adaptation-simplified]
   **Caveat**: pine64 vendors an OLD 145-line inc (`Provides: kernel` only; Image + dtbs + `modules_install` into
   /boot and /lib/modules, no boot.img, `BuildRequires: mer-kernel-checks`) and pairs it with the forked
   droid-hal-version of A4, so nothing demands `droid-hal-img-boot`. Current HelloVolla/VerdandiTeam HEAD has
   `Provides: kernel, droid-hal-kernel, droid-hal-img-boot, droid-hal-img-recovery` but (Michał 2026-10-13 "remove dtc
   dependency for mainline use", `ramdisk_directory`, `deviceinfo_append_dtb`) runs mkbootimg UNCONDITIONALLY and
   needs `deviceinfo_bootimg_header_version`; no `build_bootimg 0` switch exists. [C: raw/pine64-vendored-kas.inc vs
   ref/.../kernel-adaptation-simplified.inc lines 200-250]
6. **No droid-hal-init, no initramfs, no sailfish-boot-utils on pinephone.** `delete_file.list` removes
   droid-hal-init.service, droid-hal-startup/shutdown/init-done/load-firmware.sh, droid-late-start, media_codecs*,
   ofono/binder.conf and adaptation-community.ini from the common sparse; nothing replaces them: systemd is init.
   Boot = tow-boot (user installs) -> `/boot/boot.scr` (from `sparse/boot/boot.pinephone11.cmd`: `root=PARTUUID`,
   `booti`) -> kernel; `/boot` mounted by `sparse/etc/systemd/system-generators/boot-mount-generator.sh`. Compositor:
   `var/lib/environment/compositor/droid-hal-device.conf` with `EGL_PLATFORM=drm QT_QPA_PLATFORM=eglfs
   QT_QPA_EGLFS_INTEGRATION=eglfs_kms QT_QPA_EGLFS_KMS_CONFIG=/etc/eglfs-config.json`. Own
   `features.d/adaptation-pinephone.ini` lists the three OBS repos. [C: ref/droid-config-pinephone]
7. **Images = mic + hand-maintained .ks in a CI repo**, not in the config package:
   `gitlab.com/sailfishos-porters-ci/dont_be_evil-ci` (`image: coderus/sailfishos-platform-sdk-base:4.6.0.13`,
   `run-mic.sh`: `mic create fs --arch=$PORT_ARCH --tokenmap=ARCH:..,RELEASE:..,RELEASEMAJMIN:..,EXTRA_NAME:..
   --record-pkgs=name,url --outdir=sfe-$DEVICE-$RELEASE$EXTRA_NAME --pack-to=...tar.bz2 Jolla-@RELEASE@-$DEVICE-@ARCH@.ks`,
   `root.env`: RELEASE=5.0.0.77 PORT_ARCH=aarch64). The .ks: `part / --size 500`, repos jolla/hotfixes/apps + the
   three OBS repos + chum, `%packages` = `patterns-sailfish-device-configuration-pinephone`, `%pre` touch .bootstrap,
   `%post` = arch hack, rpm --rebuilddb, oneshot --mic, `ssu release/mode 4`, `useradd nfc radio`. Output = rootfs
   tar.bz2 (+ .ks/.packages/.urls). [C: raw/gitlab.com_*]
8. **Flashing**: `flash-it.sh` downloads the CI tarball, parted msdos boot/root/home, `tar -xpf`, copies `/boot/*` of
   the rootfs to the boot partition, renames `boot.<dev>.scr` -> `boot.scr`, dd's u-boot only on PineTab2;
   `pinephonepro-image-maker/make_release.sh` does the same into a loop `emmc.img`. U-Boot is a separate repo
   `u-boot-bootloader` (ATF + u-boot submodules, `build.sh`), not an RPM. [C: raw flash-it summary, make_release.sh]
9. **Docs convention**: a `documentation` GitHub wiki (Installation / FirstBoot / KnownIssues / NewUserTips) + the forum
   thread; old merproject wiki page `Adaptations/PinePhone64` (archived) is the HADK-like "how I did it" (U-Boot, kernel,
   rootfs, device repos). No native HADK exists; forum thread 26289 points to postmarketOS "Mainlining" wiki. [C]
10. **Parallel Pro1-X work**: `sailfish-on-fxtecpro1/{kernel-adaptation,droid-config,droid-hal-version}-halium-qx1050`
    (piggz, pushed 2026-10-03/04, Halium 4.19 kernel, header v2 boot.img, `droid-config-halium` common package) is
    published in OBS `nemo:devel:hw:fxtec:halium-qx1050`. A project directory `nemo:devel:hw:fxtec:qx1050` exists on
    the repo server (HTTP 200) but has no `sailfish_latest_aarch64` target (404), i.e. nothing published; `qx1000` and
    `t5` directories exist too. Ownership unverifiable (OBS API 503 all evening). [C/NF]
11. mer-kernel-check on the 7.1 config: ERRORs CONFIG_INET_AH, CONFIG_QFMT_V2 (+ warnings). [C: port/notes]

## B. Inferred / not found
- Victara mainline: droid-config-victara is public (native_build, `/boot/hybris-boot.img` via flash-partition, i.e.
  mainline kernel inside an Android boot.img; `kickstart/post/99_groups`), but `kernel-adaptation-victara` has no
  public repo or RPM; OBS `nemo:devel:hw:motorola:victara` is still the hybris port. [NF]
- nabu (BirdZhang) = Linux 6.14 via kernel-adaptation-simplified + hybris userspace, not a native port. [C]
- Librem 5 (forum 16625): intent only, no image. Xperia mainline on SFOS, rinigus mainline work, `sfos-image-builder`,
  `sailfish-boot-utils`, dracut-based SFOS initramfs: nothing found. [NF]
- qrtr/rmtfs/tqftpserv: not in native-common, not seen in chum (listing path unverified). [I]
- mic inside the platform-SDK image under rootless podman: not tried here. [I]

## C. Recommended layout (GitHub org e.g. `sailfish-on-qx1050`, OBS `nemo:devel:hw:fxtec:qx1050`)
```
droid-config-qx1050/        rpm/droid-config-qx1050.spec (device qx1050, vendor fxtec, native_build 1,
                            community_adaptation 1, pixel_ratio 1.5, disable_tools 1), droid-configs-device/ submodule
                            (piggz native or mer-hybris master), sparse/ (current tree MINUS etc/hw-release,
                            usr/lib/hw-release, ssu/board-mappings.d/*; PLUS compositor conf, eglfs-config.json,
                            boot-mount generator if /boot is separate), patterns/*.inc (stage1 list -> adaptation.inc;
                            Requires: kernel-adaptation-qx1050, firmware-fxtec-qx1050, qrtr, rmtfs, tqftpserv),
                            delete_file.list (copy pinephone's), .portslistinfo, kickstart/post/50_users
droid-hal-version-qx1050/   11-line spec, submodule droid-hal-version, native_build 1  -> generates hw-release
kernel-adaptation-qx1050/   rpm/kernel-adaptation-qx1050.spec, linux/ submodule (vanilla 7.1 tag or nyymi fork),
                            patches/ (nyymi's 14 diffs as a numbered series) + apply_patches 1, qx1050_defconfig
                            (= NixOS config + sfos.fragment, mer-kernel-check clean), devicetrees qcom/sm6115-fxtec-pro1x.dtb,
                            kernel-adaptation-simplified/ submodule. Ships /boot/Image, /boot/*.dtb, /lib/modules.
firmware-fxtec-qx1050/      modem/adsp/wlan/bt blobs from build/fw (licence question, see F)
qrtr, rmtfs, tqftpserv      keep the three specs as they are; target native-common (piggz) or the device project
qx1050-ci/ (or ci/ in config) Jolla-@RELEASE@-qx1050-@ARCH@.ks + run-mic.sh + root.env (clone of dont_be_evil-ci)
flash-it/ or tools/         install-on-nixos.sh, sfos-boot-once.sh, initramfs/  -> clearly labelled bringup/dual-boot
documentation wiki          Installation / FirstBoot / KnownIssues + "how it was built" page
```
Mapping: stage1-base.sh -> patterns + .ks `%packages`; stage2-device.sh steps 1b -> .ks `%post` (already identical
text), 2 -> kernel-adaptation + firmware RPMs, 2b -> patterns Requires + `%post systemctl enable` in a sparse
`*.wants/` symlink, 3 -> droid-config RPM, 4 (ssh keys, PermitRootLogin, journald) -> stays a bring-up script or a
`droid-config-qx1050-bringup` subpackage never in the release pattern; repos.sh -> .ks repo lines.

## D. Minimal reproducible set
- OBS: the three device repos + firmware + the qrtr trio in `nemo:devel:hw:fxtec:qx1050` (needs the project owner),
  `sailfish_latest_aarch64` target; CI .ks pulls them. Kernel builds on OBS with `crossbuild aarch64` only if the inc
  can skip boot.img (see F).
- Local/podman: `mb2 -t SailfishOS-5.1.0.11-aarch64 build` each repo in the SDK container, then
  `mic create fs ... --pack-to` in the `coderus/sailfishos-platform-sdk-base` image with a local repo dir; that replaces
  stage1/stage2 entirely. The one-shot extlinux swap stays a NixOS-side script.

## E. Divergences to keep and document
- U-Boot extlinux + a bind-mount initramfs is unique to this port (pine: tow-boot + boot.scr, no initramfs; victara:
  aboot + boot.img). Releasable form: rootfs tarball + flash script writing a partition and an extlinux entry.
- Kernel source currently lives in a Nix store, not git; 14 unsubmitted diffs; mer-kernel-check errors.

## F. Open questions for the upstreamer
1. Who owns `nemo:devel:hw:fxtec:qx1050`; coordinate with piggz (halium-qx1050, same device, Oct 2026).
2. Upstream a `build_bootimg 0`-style switch (or `ramdisk_directory`-based path) to kernel-adaptation-simplified so
   U-Boot devices stop vendoring the 2020 copy. Until then: a slim/pinned kernel inc + mer-hybris droid-hal-version
   fails on OBS unless the kernel spec adds `Provides: droid-hal-kernel droid-hal-img-boot droid-hal-img-recovery`
   or the dontbeevil droid-hal-version fork is used (pinephone does the latter).
3. Firmware redistribution licence (incl. nyymi's ADSP sensor blob).
4. Fix the two mer-kernel-check ERRORs in the fragment. 5. Credit: delete_file.list, compositor conf, eglfs json,
   .ks `%post`, boot-mount generator are lifted from sailfish-on-dontbeevil; qrtr specs from linux-msm.
