# jolla-camera on libcamerasrc (Pro1-X mainline): camera switching + still capture

Research report, 2026-10-24 (LLM agent, host-only; line numbers refer to files it read).
"BUILT" = the source actually compiled into the installed package, recovered from
`qt5-qtmultimedia-debugsource-5.6.2+v4l.20230905210651.c98eb6d0-1.4.17.bso.aarch64.rpm`
(repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/aarch64/).

## 0. Which qtmultimedia is installed (confirmed)
- Commit c98eb6d0 is NOT on GitHub under sailfishos/, sailfishos-native/, piggz/,
  sailfish-on-dontbeevil/ (it is mlehtima/qtmultimedia branch `v4l`, see research-camera-sink.md).
  tar_git version string says branch `v4l`, commit date 2023-09-05.
- Diff BUILT vs upstream sailfishos/qtmultimedia `mer-5.6` HEAD (791ba20e, 2026-05): BUILT =
  upstream as of the 2021-10 pulsesrc commit (4926fe50) + one 3-line hunk in qgstutils.cpp (probe
  camera set to READY before reading sensor-mount-angle). The 2026 JB#64203 startup commits (and
  `CAMERA_STARTUP_LOG`) are NOT in BUILT.
- The GitHub fork sailfishos-native/qtmultimedia `mer-5.6` (58988e91, 2019 "Enable V4L support")
  is older than BUILT; its "primary"/"secondary" hard-coding in qgstutils.cpp is 2019 upstream
  code, NOT what is installed. "+v4l" in practice means only: the spec drops
  `"DEFINES+=DISABLE_V4L"` from the qmake line, so `qtCompileTest(linux_v4l)` runs
  (qtmultimedia.pro:32-34) and `USE_V4L` is defined (camerabin.pro:88-89, gsttools.pro:139).
  No source patches beyond that.

## 1. Front/back selection

### (a) Device list (BUILT src/gsttools/qgstutils.cpp:513-640)
1. `enumerateCameras(factory)` first checks the camera-source factory class. Only if it has a
   `camera-device` int property (droidcamsrc/subdevsrc) are devices "0".."max" synthesised with
   position from `sensor-direction` (:533-569). wrappercamerabinsrc has `video-source` instead
   -> `hasVideoSource=true` (:570), list stays empty, falls through (:575-579).
2. `#ifdef USE_V4L` (:582): scans `/dev/video*`, `open(O_RDWR)`, `VIDIOC_ENUMINPUT` loop; for
   the first CAMERA-type input: `isCamera = ::ioctl(fd, VIDIOC_S_INPUT, input.index) != 0;`
   (:602). Quirk: `input.index` is passed BY VALUE where `_IOWR('V',39,int)` expects a pointer;
   with index 0 the kernel's copy_from_user(NULL) fails -> -EFAULT -> `isCamera = true`. So
   every node whose driver answers ENUMINPUT with a CAMERA input is listed. CAMSS does (linux
   v6.12 drivers/media/platform/qcom/camss/camss-video.c:535-557 `video_enum_input` ->
   "camera"). Expected list on the Pro1-X: one entry per CAMSS video node, name=`/dev/videoN`,
   description=card, driver=`qcom-camss`, orientation 0, **position UnspecifiedPosition**
   (:626-631). No GstDeviceMonitor, no env var, nothing libcamera-aware.
3. Default device = index 0 (qgstreamervideoinputdevicecontrol.cpp:76-79;
   camerabinserviceplugin.cpp:98,106).

### (b) Property set on the source (BUILT camerabinsession.cpp:464-553)
- `setDevice(QString)` (camerabinservice.cpp:111-115) stores `m_inputDevice`;
  `buildCameraSource()` only acts `if (m_cameraSrc && !m_inputDevice.isEmpty())` (:488). First
  time it creates the inner source from `QT_GSTREAMER_CAMERABIN_VIDEOSRC` (:510-525; the
  `drv=elem,...` form keys on the v4l2 driver string via `QGstUtils::cameraDriver`, :519), else
  `v4l2src` (:529), and installs it with `g_object_set(m_cameraSrc, "video-source", ...)` (:532).
  Every call then does `g_object_set(m_videoSrc, "device", m_inputDevice.toUtf8(), NULL)` (:536)
  -> the "no property named 'device'" warning from the port log; nothing else is set. The
  `camera-device` int branch (:538-539) is only for droid/subdev sources.
- libcamerasrc 0.4.0 selects by `camera-name` only (gstlibcamerasrc.cpp:157, 741-744, 959-965),
  read in `gst_libcamera_src_open` at NULL->READY (:384-398; change_state :783-785); without it,
  the first camera from `cm->cameras()` (:399-).

### (c) Position
- v4l path: never (UnspecifiedPosition, qgstutils.cpp:629); `cameraPosition()` is a lookup (:660-667).
- jolla-camera (sailfishos/jolla-camera master, src/capture/CaptureView.qml) needs it: :621-640
  builds `backFacingCameras`/`frontFacingDeviceId` only from entries with
  `position === Camera.FrontFace/BackFace`; `_cameraIds` (:58-70) is empty otherwise and
  `switchToNextCamera()` returns early (`_cameraIds.length < 2`, :211-222). So today the switch
  is a no-op because of the missing position, not only because of the property name.

### (d) Env variables read by the plugin (grep qgetenv in BUILT)
`QT_GSTREAMER_CAMERABIN_SRC` (camerabinserviceplugin.cpp:139; camera-source factory, defaults
`subdevsrc`, `wrappercamerabinsrc` :143), `QT_GSTREAMER_CAMERABIN_VIDEOSRC`
(camerabinsession.cpp:510), `QT_GSTREAMER_CAMERABIN_FLAGS` (:176). Elsewhere in qtmultimedia:
`QT_VIDEONODE`, `NEMO_RESOURCE_DISABLE`, `NEMO_RESOURCE_CLASS_OVERRIDE`,
`QT_GSTREAMER_PLAYBIN_FLAGS`, `QT_GSTREAMER_USE_PLAYBIN_VOLUME`, `QT_GSTREAMER_WINDOW_VIDEOSINK`,
`QT_DEBUG_PLUGINS`. Debug aid: `GST_DEBUG_DUMP_DOT_DIR` makes the plugin dump `camerabin*.dot`
on state changes/errors (:1114-1115, :1166-1167; `CAMERABIN_DEBUG_DUMP_BIN 1`).

### A contradiction to settle on the device first
Under BUILT, a non-empty device list (required for the observed 'device' warning) also means
`supportedResolutions()` queries libcamerasrc's src pad (:1289-1298 -> `gst_pad_query_caps`),
which has no caps-query handler (gstlibcamerapad.cpp:66-77) and so returns the template
`video/x-raw; image/jpeg; video/x-bayer` (gstlibcamerasrc.cpp:170-180) with no width field ->
`G_VALUE_TYPE(NULL)` at :1456-1463 should crash jolla-camera inside `CameraConfigs::handleStatus`
(jolla-camera src/cameraconfigs.cpp:63-91, runs at LoadedStatus). The log says it stayed up.
Alternative reading: list empty -> env never consulted (:488 false), and wrappercamerabinsrc's
default `autovideosrc` (gstwrappercamerabinsrc.c:486) picks libcamerasrc anyway (rank PRIMARY,
gstlibcamera.cpp:15). Discriminator: run jolla-camera with `GST_DEBUG_DUMP_DOT_DIR=/tmp/dot`;
an element named `camera_source` of type GstLibcameraSrc = list non-empty; a GstAutoVideoSrc
wrapping libcamerasrc = list empty. Also `v4l2-ctl -d /dev/videoN --list-inputs` per node, and
a QML scratch printing `QtMultimedia.availableCameras` (deviceId/position) and
`camera.imageCapture.supportedResolutions`.

### Smallest change (proposal, untested)
No env/dconf trick exists: device names and positions come only from the plugin; the `drv=elem`
form of `QT_GSTREAMER_CAMERABIN_VIDEOSRC` keys on the v4l2 driver string, identical
(`qcom-camss`) for all nodes. Patch BUILT qtmultimedia at two sites:
1. qgstutils.cpp `enumerateCameras`, in the `hasVideoSource` branch before the `#ifdef USE_V4L`
   scan (:575): if `qgetenv("QT_GSTREAMER_CAMERABIN_VIDEOSRC")` names a factory whose class has
   a `camera-name` property, enumerate via
   `gst_device_provider_factory_get_by_name("libcameraprovider")` +
   `gst_device_provider_get_devices()` and emit `CameraInfo{ name=id, description=id,
   orientation, position, driver="libcamera" }`. The provider sets `name`/`display-name` to
   `camera->id()` (gstlibcameraprovider.cpp:148-152), registered at rank PRIMARY
   (gstlibcamera.cpp:17-18). Position: 0.4.0's GstDevice carries no `properties::Location`
   (provider sets only name/display-name/caps/device-class), so take it from a mapping env,
   e.g. `QT_GSTREAMER_CAMERABIN_LIBCAMERA_FRONT=<id>`, else index 0=Back, 1=Front (the 2019
   fork hard-coded exactly that in 58988e91). Orientation is likewise unknown (0); jolla-camera
   uses it for viewfinder rotation/mirroring (CaptureView.qml:28-36, 128-129) -> second env or
   constant.
2. camerabinsession.cpp:536: `if (g_object_class_find_property(G_OBJECT_GET_CLASS(m_videoSrc),
   "camera-name")) g_object_set(m_videoSrc, "camera-name", ...) else g_object_set(...,
   "device", ...)`.
The switch then needs no further code: `QDeclarativeCamera::setupDevice` unloads first
(qdeclarativecamera.cpp:384-387 `setCameraState(UnloadedState)` before `setSelectedDevice`),
the plugin's `unload()` sets camerabin to NULL (upstream :947; same code in BUILT), `load()` ->
`setupCameraBin()` -> `buildCameraSource()` re-sets the property on the existing inner element,
and NULL->READY re-runs `gst_libcamera_src_open` with the new name. Caveat: `unload()` defers
NULL while `m_busy` (capture in flight). Camera ids: `cam -l`. Build: needs a qtmultimedia build
(OBS native-common or Platform SDK); the +v4l spec line is the only packaging delta to carry.

## 2. Still image capture

Verified in source:
- Capture = `g_object_set(camerabin,"location")` + `g_signal_emit_by_name(camerabin,
  "start-capture")` (BUILT camerabinsession.cpp:555-574). `post-previews` is forced FALSE (:181),
  so `imageCaptured` comes from the plugin's own pad probe on the jpegenc sink
  (camerabinimagecapture.cpp:311-330) and `imageSaved` from the `image-done` element message
  (:355-375). Probes attach by element-name substring `jpegenc` / `jifmux` / `metadatamux`
  (:311, :332); encodebin's autoplug names (`jpegenc0`) satisfy this.
- camerabin2 without GstPhotography works: wrappercamerabinsrc's `start_image_capture` calls
  `gst_photography_prepare_for_capture` only if the inner bin exposes GstPhotography, else
  `gst_wrapper_camera_bin_reset_video_src_caps(self, image_capture_caps)`
  (gstwrappercamerabinsrc.c:743-793); the still is one frame routed to `imgsrc` (:756).
  Default image profile is an `image/jpeg` encoding profile -> encodebin -> jpegenc
  (gstcamerabin2.c:1594-1604).
- Elements per SFOS specs: `libgstjpeg.so` (sailfishos/gst-plugins-good spec:138);
  `libgstcamerabin.so`, `libgstjpegformat.so` (jifmux), libgstphotography
  (sailfishos/gst-plugins-bad spec :156,165,201,209).
- Caps requested: `setupCaptureResolution()` (BUILT :331-396) puts `imageCapture.resolution`
  into `image-capture-caps` (:387) and, because `m_usingWrapperCameraBinSrc`, forces the
  viewfinder caps to the same size (:339-344, :396; comment: wrappercamerabinsrc cannot carry
  different caps on imgsrc/vidsrc/vfsrc). Good for libcamerasrc: no mid-stream renegotiation at
  shutter time. (libcamerasrc 0.4 can renegotiate: `task_run` does `cam_->stop()`/negotiate/
  `start()` on a reconfigure, gstlibcamerasrc.cpp:545-567, but that is the stop/teardown path
  that aborted in the port log.)
- jolla-camera's resolution choice (master): no dconf resolution keys any more; the
  `[apps/jolla-camera/primary/image] imageResolution=...` keys seen in mer-hybris droid-config
  repos are the old scheme. Now `CameraConfigs` reads `QCameraImageCapture::supportedResolutions()`
  and splits it by dconf `/apps/jolla-camera/maxImageResolution` (cameraconfigs.cpp:83-96;
  `maxVideoResolution` :108); CaptureView.qml:651-654 picks the largest size matching
  `Settings.aspectRatio` (tolerance 0.05, :288-310) or, with `enableHighres`, the largest
  "highres" size. Shipped dconf/00-jolla-camera.txt has no resolution keys. `_pickResolution`
  returns "-1x-1" (= unconstrained caps) if nothing matches the ratio.
- `supportedResolutions()` (BUILT :1423-1500) reduces caps to min/max per structure; with
  libcamerasrc as `m_videoSrc` it sees template caps (crash note above); via camerabin's
  `image-capture-supported-caps` it sees videoscale ranges (1..32767) -> `1x1, 32767x32767`,
  ratio match fails -> "-1x-1" -> libcamera picks its own default size. Either way the
  advertised list is synthetic, not libcamera's mode list.

Pitfalls (inferred, not device-tested):
1. Exact-size requests: libcamerasrc fixates on the first peer structure, `validate()` may
   Adjust, and it pushes the adjusted caps (gstlibcamerasrc.cpp:440-470); a fixed-size
   `src-capsfilter` directly after the source (gstwrappercamerabinsrc.c:525-526, 569-571) then
   fails negotiation. Request only sizes `cam -c N -I` / `gst-device-monitor-1.0` list; steer
   with `maxImageResolution` in dconf; keep `enableHighres` off until a size is confirmed.
2. The always-pad's role is VideoRecording (gstlibcamerapad.cpp:120-123), not StillCapture:
   stills are video-stream frames; one stream per pad, which matches camerabin's single pad.
3. `image-done` never arriving: with post-previews off, the usual culprits are an encoder not
   named `jpegenc*` (not the case here) or multifilesink `location` unwritable (jolla-camera
   passes `Settings.photoCapturePath('jpg')`, CaptureView.qml:524, under sailjail Pictures
   permission).
4. Orientation/EXIF: plugin orientation is 0 for v4l devices; jolla-camera writes metadata via
   `captureOverlay.writeMetaData()` (:521) using `camera.orientation` -> expect wrong rotation
   until the enumeration patch supplies it.

## 3. Prior art (mostly negative)
- piggz PinePhone: sailfish-on-dontbeevil/droid-config-pinephone issue #55 ("Camera with
  libcamera", 2020-11) is kernel/libcamera bring-up; no jolla-camera/camerabin content. That
  repo's camera bits are `cameras_setup.sh` (media-ctl for ov5640/gc2145) only; no
  `QT_GSTREAMER_CAMERABIN_*` in its sparse tree. piggz/harbour-advanced-camera is a
  QtMultimedia camerabin app (spec: Qt5Multimedia, no libcamera). piggz/harbour-shutter uses
  libcamera directly (README) and points at nemo:devel:hw:native-common for libcamera; "tested
  on original PinePhone only".
- libcamera-devel: searches for camerabin/wrappercamerabinsrc on lists.libcamera.org returned
  nothing relevant (2020 plugin series; a 2025 "property to control orientation" patch,
  post-0.4).
- sailfishos-native org has no camera-app repo; its qtmultimedia is the 2019 fork above.

## URLs
https://github.com/sailfishos/qtmultimedia (mer-5.6) | https://github.com/sailfishos-native/qtmultimedia |
https://github.com/sailfishos/jolla-camera | https://gitlab.freedesktop.org/camera/libcamera (v0.4.0
src/gstreamer/) | https://gitlab.freedesktop.org/gstreamer/gstreamer (1.24: gst-plugins-bad/gst/camerabin2,
gst-libs/gst/basecamerabinsrc) | https://github.com/sailfishos/gst-plugins-good,
https://github.com/sailfishos/gst-plugins-bad (rpm specs) | linux v6.12
drivers/media/platform/qcom/camss/camss-video.c |
https://github.com/sailfish-on-dontbeevil/droid-config-pinephone/issues/55 |
https://github.com/piggz/harbour-shutter, https://github.com/piggz/harbour-advanced-camera |
debugsource RPM: https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/aarch64/qt5-qtmultimedia-debugsource-5.6.2+v4l.20230905210651.c98eb6d0-1.4.17.bso.aarch64.rpm
