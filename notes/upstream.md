# Items worth sending upstream (found during the Pro1-X mainline bring-up, 2026-10)

Each entry: what, where, evidence. None sent yet; the human author decides and signs.

## sailfishos/ngfd
- `50-ffmemless.ini` lists 17 `supported_effects` names; ff-memless has 16 slots, so on any
  plain FF_RUMBLE driver the 17th upload fails with ENOSPC and ngfd exits ("Could not load
  ngfd effects"). Same as PR #3 (2021, closed). One name is a typo (`drag_boundary_drag_end`),
  two effects silently fall back to the default. Fix options: shorter stock list, or survive
  ENOSPC in `ffm_setup_effects`.

## PulseAudio (Jolla fork, 17.0+git1)
- Aborts in `pa_alsa_ucm_set_port()` ("Assertion 'dev == data->device' failed") when two UCM
  verbs declare a device of the same name. Upstream fix: ed3d4f08 (2024-01, "alsa-ucm: Replace
  port device UCM context assertion with an error"). piggz's 17.0+git2 master snapshot has it.
  Ask Jolla to pick it (JB ticket) so mainline ports can use stock pulseaudio.

## sailfishos/mce
- `SW_KEYPAD_SLIDE` is read as 1 = closed (N900 convention); `input-event-codes.h` defines
  1 = slid out, which mainline gpio-keys follows. No inversion setting exists, so a mainline
  kernel must lie in its DT (this port does, sfos-dt.patch). Proposal: `[EVDEV]` key to invert
  the slide polarity, or follow the kernel definition with a compatibility flag.
- `mce --verbose` on a DCS panel backlight logs `can't flush .../brightness: EINVAL` after
  blanking (harmless) and the dim fade shows one jump to full brightness (open, panel
  panel-boe-bf060y8m-aj0).

## Linux (mainline / sm6115-mainline tree)
- `hci_uart_qca` / WCN3990 on the Pro1X: works when enabled from boot; after a power-down it
  never answers again ("command 0xfc00 tx timeout", "Retry BT power ON"), and unbinding the
  serdev shows `WARNING drivers/regulator/core.c _regulator_put` (enable count unbalanced).
- sm4250 sound card (sm8250.c machine driver, WCD937x, AW87339 amps): on some kernel builds
  the first card probe streams silence although every DAPM widget and the amp enable GPIOs
  are on; re-creating the card (`rmmod/modprobe snd_soc_sm8250`) after PulseAudio has used
  it makes it play. Not a config option (bisected); boot-time ordering. Root cause open.
- Pro1X DT (nyymi609 patches): keyboard backlight GPIO 103 is active-low on the hardware
  (declared ACTIVE_HIGH); `sm6115-fxtec-pro1x.dts` upstream hall switch is ACTIVE_HIGH and
  reports 1 = open correctly, keep it (the SFOS inversion is a workaround for mce).
- Panel `panel-boe-bf060y8m-aj0`: 165268 kHz gives 61.00 Hz and touch ghost events; 162559
  kHz is exactly 60 Hz (nyymi609's pro1x-dsi.patch, to be sent by its author).

## kernel-adaptation-simplified (HelloVolla / VerdandiTeam)
- Needs a switch to skip `mkbootimg` and ship Image + dtbs + modules for U-Boot/extlinux
  devices (pine64 carries a 2020-era copy of the inc for that).

## fxtec-pro1x-nixos (nyymi609)
- Haptics driver defaults (gain 6, drive 59) are below what one feels through the case;
  gain 7 / drive 100 is a better default.
- Venus (SM6115 AR50LT) is not enabled; the SFOS kernel backports the upstream node
  (Dmitry Baryshkov's series, DT patches applied by Bjorn Andersson on 2026-07-08) as `device/kernel/patches/sfos-venus.patch`.
  Once the base kernel carries that node, only the board `&venus` block remains, and it belongs
  in the pro1x DT for NixOS too.
- Venus firmware: the Fxtec vendor image `qcom/sm6115/Fxtec/QX1050/venus.mbn` is
  VIDEO.VE.6.0-00049; the venus driver requires >= 6.0.55 on QCM2290/SM6115 (encoder needs
  secure buffers below 6.0.54) and fails the probe ("Firmware v6.0.49 < v6.0.55", -EINVAL).
  linux-firmware's `qcom/venus-6.0/venus.mbn` (6.0-00055, the driver default) loads on the
  Pro1-X, so the board node needs no `firmware-name` (the device accepts that signature).
- The panelgap patches are broken (seen earlier in this port); a LICENSE / Signed-off-by for
  the patch series would help any redistribution.

## sailfishos/qtmultimedia (mer-5.6; also mlehtima's v4l branch used by native-common)
- `CameraBinSession::supportedCaps()` returns the inner source's pad caps unchecked; with a
  source that has no caps query handler (libcamerasrc 0.4) that is the pad template without
  width/height/framerate, and `supportedResolutions()`/`supportedFrameRates()` dereference
  the missing fields (`g_value_init(&w, G_VALUE_TYPE(oldW))`, NULL → SIGSEGV, confirmed by a
  core dump of jolla-camera on the Pro1-X). `patches/qtmultimedia/0001-…` falls back to
  camerabin's supported-caps properties when the pad caps carry no size.
- The fallback has no sizes either, so jolla-camera finds no resolutions and requests none;
  the source then runs at its largest size in every mode (3840x2400 viewfinder, no 1080p
  video). `0005-…` takes the sizes from the libcamera GstDevice caps. Cleaner upstream fix:
  a caps query handler in libcamerasrc (below).
- `QVideoSurfaceGstDelegate::render()` returns GST_FLOW_ERROR when the GUI thread misses its
  300 ms deadline once, which stops the whole camerabin pipeline (frozen viewfinder after
  about a second with a slow source). `0003-…` drops the late frame instead.
- The session connects `notify::ready-for-capture` but never reads the initial value; a
  source that is ready from the start and never toggles (wrappercamerabinsrc around
  libcamerasrc) leaves the shutter disabled forever. `0004-…` reads it once after connecting.
- Camera enumeration knows only /dev/video* (v4l2src); `0002-…` enumerates libcamera cameras
  through `libcameraprovider` and selects them by `camera-name`.

## jolla-camera / sailjail profile
- The sandbox whitelists specific /usr/share dirs; /usr/share/libcamera (IPA modules'
  tuning files) is hidden, the software ISP then fails to load and the viewfinder stays
  black. Port workaround: `whitelist /usr/share/libcamera` in
  `/etc/firejail/disable-common.local`. Upstream: add it to the Camera permission.
- Video recording on a native (non-hybris) port needs encoders the hybris stack takes from
  droidmedia: H.264 from Venus through v4l2h264enc, AAC from gst-libav's `avenc_aac`, which
  is registered with rank none and therefore never chosen by encodebin (port sets
  `GST_PLUGIN_FEATURE_RANK=avenc_aac:primary` in the session environment).

## sailfishos/gst-plugins-good
- The spec builds video4linux2 with `-Dv4l2-probe=false`, which compiles out the registration
  of the stateful V4L2 codec elements (v4l2h264enc, v4l2h264dec, ...). Hybris devices encode
  through droidmedia; native ports with a stateful V4L2 codec (Qualcomm Venus)
  get no hardware encoder. `patches/gst-plugins-good/0001-…` flips it (cost: every /dev/video*
  is opened once when the registry is rebuilt).

## libcamera
- `libcamerasrc` has no caps query handler on its src pad: before negotiation it answers
  with the size-less template. Applications that ask the source for its sizes (Qt camerabin)
  see nothing. The device provider already computes the right caps.

## sailfishos/sailjail, sailfishos/firejail
- `fcopy` (private-etc copy) aborts the whole sandbox when a file's owner has no passwd entry
  ("Error fcopy: getpwuid /etc/pulse", "check: getpwuid: Success"); the booster then dies in a
  restart loop and the UI only says "application specific booster is not available". A stray
  uid in /etc (a config package built with wrong ownership, a file copied by hand) takes
  Camera and Browser down with no pointer to the cause. Proposal: copy the file root-owned and
  warn, or at least name the file and the uid in the error.

## Linux (camera, found 2026-10-24)
- S5K4H7 (nyymi609's driver): the probe sometimes fails with "Error reading reg 0x0000: -110"
  (I2C timeout) at boot and succeeds on a later bind: power-up/settle timing in the driver.
  CAMSS's async notifier then registers no camera at all until every sensor is bound.
- libcamera 0.4 `libcamerasrc`: aborts in the CameraManager thread at EOS/teardown with
  gst-launch (identity eos-after); frames are delivered before that.
