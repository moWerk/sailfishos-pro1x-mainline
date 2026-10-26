# Packages installed on the device from outside Jolla's 5.1.0.11 repositories

Installed by hand during the bring-up (rpm -Uvh / pkcon install-local); stage 1 must grow
these (as repo entries + pattern lines) when the rootfs is rebuilt. Removed packages at the end.

| Package (exact file) | Repo | Reason |
|---|---|---|
| pulseaudio-17.0+git2+master.20260702210131.1.g8266efb33-1.1.1.bso | nemo:devel:hw:pine:dontbeevil | UCM set_port assertion fix (replaces Jolla's 17.0+git1) |
| ofono-2.12.0+git1-1.90.3.bso, ell-0.70+git1-1.14.3.bso | nemo:devel:hw:pine:dontbeevil | QRTR modem driver (replaces ofono 1.29) |
| libqmi-1.34.0+git1-1.1.3.bso, libqmi-utils (qmicli), libqrtr-glib-1.2.0+git1-1.1.3.bso | nemo:devel:hw:native-common | UIM provisioning session at boot |
| libcamera-0.4.0+git1-1.19.13.bso, libcamera-tools (installed --nodeps: gtest missing), libevent-2.1.8+main… | nemo:devel:hw:native-common | camera (libcamerasrc, cam) |
| libyaml-0.2.5+git2, elfutils-libelf + elfutils-libs 0.193, gstreamer1.0-tools 1.26.11, libsailfishapp-launcher 1.2.17 | jolla (oss) | libcamera deps, gst-launch, sailfish-qml (tests) |
| alsa-utils, alsa-ucm-conf, pulseaudio-module-keepalive, pulseaudio-modules-nemo-parameters, pulseaudio-policy-enforcement, policy-settings-common, ohm-plugin-*, mce-tools, libngf-qt5-qtfeedback | jolla | audio stage A/B, vibration |

Repo base URLs: https://repo.sailfishos.org/obs/nemo:/devel:/hw:/pine:/dontbeevil/sailfish_latest_aarch64/aarch64/
and https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/aarch64/
(the listing needs a browser User-Agent; the files download plainly). The native-common
`qt5-qtmultimedia-*-5.6.2+v4l` fork was already pulled by stage 1 through repo priority.

## Built here with the mb2 engine (sfos-sdk-setup container, see README "How a session works")

| Package | Source | Why |
|---|---|---|
| qt5-qtmultimedia 5.6.2+v4l.20261024000000.qx1050-10 (all installed subpackages) | mlehtima/qtmultimedia branch v4l at c98eb6d0 + `patches/qtmultimedia/0001-…` | camerabin plugin crash with libcamerasrc (null size in `supportedResolutions`); later releases: libcamera enumeration, sizes, viewfinder orientation and front camera mirror (R10) |
| libcamera 0.7.2-1.qx1050 (+tools) | upstream v0.7.2, packaging from mlehtima/libcamera (native-common), qcam/lc-compliance/tests off | `PipelineHandler::stop()` assertion of 0.4.0 on camera unload (software-ISP stop fixes landed in 0.5.0, request completion on stop in 0.6.0) |
| libcamera 0.7.2-3.qx1050 (+tools) | as -2 (imx582/s5k4h7 sensor helpers) + Milan Zamazal's RFC "Autofocus basics for software ISP" (patchwork series 6276, 2026-10-21) backported, + a sharpness statistic (0010) | autofocus for the imx582/dw9800; verified 2026-10-25: cam converges (sharpness 150 at infinity, peak ~1120 at lens 448 for a near object), jolla-camera focuses near and far, sharp photos; first focus takes a few seconds |
| protobuf-c 1.5.2-1, libssc 0.4.4-1 (+tools), hexagonrpc 0.5.0+git40e5041-1 | port/packages/ (see notes/sensors-design.md) | ADSP sensors (SSC): registry daemon + client library; installed 2026-10-26, daemon not enabled |
| gstreamer1.0-plugins-good 1.26.11+git1-1.14.2.qx1050 | sailfishos/gst-plugins-good with `-Dv4l2-probe=true` (`patches/gst-plugins-good/`) | v4l2h264enc (Venus) for video recording |

## Pitfalls met on the way (camera, 2026-10-25)

- **libcamera-qx1050 is built from the patched tree, not from the spec.** `mb2` builds the
  source directory as it is; the `Patch:` lines in `rpm/libcamera.spec` only document what the
  tree already carries. A patch that is only listed is silently not built (the first 0.7.2-3
  build had no autofocus code, and the tuning file's `Af:` entry then made the IPA fail to load:
  "Algorithm 'Af' not found", no software ISP, no camera). Apply with `patch -p1` in the tree
  and keep the spec list in step.
- **The RFC series alone does not focus.** It computes no sharpness value (`variance` stays 0),
  moves the lens only on pipelines without a frameStart signal, and lets the algorithm act on
  stale stats on 3 of 4 frames; 0008 and 0010 in the backport cover these.
- **Focus "worked" on one boot by accident.** A blur/sharp difference between boots came from
  where the lens happened to sit, not from autofocus: libcamera 0.7.2 had no lens control at
  all. A manual sweep (v4l2-ctl `focus_absolute` with the subdev held open) shows position
  ~200 sharp at 1-2 m. `media-ctl -p` does not print ancillary (lens) links; "0 links" on the
  dw9800 entity is normal.
- **The camera "shutter clack" on mainline is the dw9800 lens**, not a sound (audible from the
  camera bump with all volume off): the driver's power-up/park ramp (16 codes per 1 ms) is a
  near jump for the voice coil, and libcamera opens the lens several times on camera start.
- **v4l-utils on SailfishOS:** none in the repositories; bind-mount the NixOS store
  (`mount --bind /mnt/nixos/nix /nix`) and run v4l2-ctl/media-ctl from it.

The first qtmultimedia build used Version `…+v4l.20261024.qx1050`, which rpm sorts *below*
native-common's `…+v4l.20230905210651…` (segments compare numerically); installed with
`--oldpackage`, the tree now carries the 14-digit stamp.

## Removed on purpose

- `nemo-qtmultimedia-plugins-gstvideotexturebackend` (rpm -e --nodeps): the hybris QML video
  node backend that requests `droideglsink`; without it Qt's own video renderer backs
  `VideoOutput`, which is what libcamerasrc needs. A packaged port drops it from the pattern.
- `qt5-feedback-haptics-ffmemless`: bypasses ngfd and eats memless slots; libngf-qt5-qtfeedback instead.
- `ohm-configs-default` could not be removed (ohm requires "ohm-configs"); the template's
  `etc/ohm/*` files from droid-hal-configs are shipped on top.
