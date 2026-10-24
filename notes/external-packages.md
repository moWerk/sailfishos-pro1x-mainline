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
| qt5-qtmultimedia 5.6.2+v4l.20261024000000.qx1050 (all installed subpackages) | mlehtima/qtmultimedia branch v4l at c98eb6d0 + `patches/qtmultimedia/0001-…` | camerabin plugin crash with libcamerasrc (null size in `supportedResolutions`) |
| libcamera 0.7.2-1.qx1050 (+tools) | upstream v0.7.2, packaging from mlehtima/libcamera (native-common), qcam/lc-compliance/tests off | `PipelineHandler::stop()` assertion of 0.4.0 on camera unload (software-ISP stop fixes landed in 0.5.0, request completion on stop in 0.6.0) |

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
