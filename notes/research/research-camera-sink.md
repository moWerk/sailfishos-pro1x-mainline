# Camera viewfinder on mainline SFOS 5.1 (Pro1-X): sink research, 2026-10-24
Sources: scratchpad/src/ clones of sailfishos/qtmultimedia@mer-5.6 791ba20, mlehtima/qtmultimedia@v4l c98eb6d, sailfishos/nemo-qtmultimedia-plugins 7c5f12f,
sailfishos/jolla-camera 3c71ae6, gst-droid, nemo-gst-interfaces, piggz/harbour-shutter@main 84220ca; upstream gstcamerabin2/wrappercamerabinsrc/viewfinderbin (1.28), gstlibcamerasrc (v0.4.0); OBS public API.

## 1. The "+v4l" qtmultimedia and where droideglsink comes from
- CONFIRMED: OBS nemo:devel:hw:native-common/qtmultimedia `_service` = tar_git https://github.com/mlehtima/qtmultimedia.git branch `v4l` (NOT sailfishos-native,
  whose qtmultimedia is a dead 2019 fork). Branch = sailfishos mer-5.6 at 881274b (2021-12-03) + ONE commit c98eb6d "[media] Enable v4l support":
  rpm/qtmultimedia.spec drops `"DEFINES+=DISABLE_V4L"` (qtmultimedia.pro:32 gates config_linux_v4l -> USE_V4L). `20230905...` = tar_git stamp. No sink change at all.
- CONFIRMED: `droideglsink` appears nowhere in qtmultimedia (either branch), jolla-camera or the Sailfish gstreamer forks. It is requested by package
  nemo-qtmultimedia-plugins-gstvideotexturebackend (qt5/plugins/video/declarativevideobackend/libgstnemovideotexturebackend.so), videotexturebackend.cpp:508
  `gst_element_factory_make("droideglsink", NULL)`; with m_sink null its init() returns false (~:612) and QML VideoOutput falls back
  (qtmultimediaquicktools/qdeclarativevideooutput.cpp:230-247) to QDeclarativeVideoRendererBackend -> camerabin QGstreamerVideoRenderer ->
  QVideoSurfaceGstSink (software frames) -> scene-graph yuv/rgb nodes (qsgvideonode_yuv.cpp:49: I420/YV12/NV12/NV21; rgb: BGRx/RGB16). The warning is benign.
- CONFIRMED env vars: QT_GSTREAMER_CAMERABIN_SRC (camerabinserviceplugin.cpp:139, fallback "subdevsrc","wrappercamerabinsrc"); QT_GSTREAMER_CAMERABIN_VIDEOSRC
  (camerabinsession.cpp:552-584, sets `video-source` of wrappercamerabinsrc, default v4l2src); QT_GSTREAMER_CAMERABIN_FLAGS; QT_VIDEONODE (node priority).
  QT_GSTREAMER_WINDOW/WIDGET_VIDEOSINK are overlay/widget only. There is NO *CAMERABIN_VIDEOSINK / *_VIDEOSINK, no nemovideotexture/qtvideosink/waylandsink choice.
- INFERRED (strong): the blocker is `QT_GSTREAMER_CAMERABIN_SRC=libcamerasrc`. camerabin2 links the camera-source's `vfsrc`/`imgsrc`/`vidsrc` pads and sets
  mode/zoom (gstcamerabin2.c:1749-1787); libcamerasrc is a plain GstElement with pads `src`/`src_%u`, prop `camera-name` (gstlibcamerasrc.cpp:164,174,179,959).
  Link fails -> QCamera error -> jolla-camera CaptureView.qml onErrorCodeChanged `_unload = true` -> idles silently. Correct knob: unset _SRC, set
  `QT_GSTREAMER_CAMERABIN_VIDEOSRC=libcamerasrc`; wrappercamerabinsrc needs only a static "src" pad (gstwrappercamerabinsrc.c:473,498) and viewfinderbin
  inserts videoconvert+videoscale by default (gstviewfinderbin.c:53,206,213). Expect a harmless GLib warning from `g_object_set(videoSrc,"device",..)` (:595).
- INFERRED gate: _VIDEOSRC is only applied when enumerateCameras found >=1 device (`!m_inputDevice.isEmpty()`). Without a `camera-device` prop it scans
  /dev/video* and counts a node iff VIDIOC_ENUMINPUT succeeds (qgstutils.cpp:579-634; the S_INPUT check passes an int by value, so it "passes" by failing --
  same as upstream 5.6, fixed 5.15:651-652). camss nodes may not implement ENUMINPUT. With zero devices wrappercamerabinsrc falls back to autovideosrc
  (gstwrappercamerabinsrc.c:486); libcamerasrc is registered GST_RANK_PRIMARY "Source/Video" (gstlibcamera.cpp:15, gstlibcamerasrc.cpp:949), so it may be
  auto-picked even then. INFERRED: jolla-camera needs no adaptation dconf for a viewfinder: `deviceId: Settings.deviceId` (CaptureView.qml:573, default "")
  and CameraConfigs.ready flips at LoadedStatus (cameraconfigs.cpp:205/218); victara's jolla-camera-hw.txt (2026-07-13) only sets resolutions.
- Env delivery: jolla-camera is booster-launched (50-jolla-camera.conf -> booster-silica-media@jolla-camera.service); shell exports never reach it. Hybris
  pattern: droid-hal-configs sparse/var/lib/environment/nemo/60-multimedia.conf (CAMERABIN_SRC=droidcamsrc, PLAYBIN_FLAGS=0, CAMERABIN_FLAGS=15).
  droid-config-pinephone / -victara set no QT_GSTREAMER_* at all; pinephone only runs cameras_setup.sh (media-ctl) + v4l-utils; both require
  nemo-gstreamer1.0-interfaces, gst-droid commented out. Second silent gate to watch in the journal (not asserted): resourcepolicy plugin m_available.
- Pine state: forum "The Pinephone Thread" p2/p3 (DrYak, Oct/Nov 2023): only Shutter works, it "directly talks to the camera using libcamera", not the
  Sailfish camera API; no working jolla-camera viewfinder reported. Mainline C2 thread (affe_null 2025): libcamera + Shutter, "not integrated into the UI".

## 2. How QML VideoOutput is backed on Sailfish
- CONFIRMED: interface = nemo-gst-interfaces (pkg nemo-gstreamer1.0-interfaces; nemovideotexture.h acquire/bind/release_frame + nemoeglimagememory.h).
  Implementers (GitHub search NEMO_GST_TYPE_VIDEO_TEXTURE outside the interface repo): only gst-droid forks' droideglsink/droidvideotexturesink and legacy
  gst-jolla. Consumers: the videotexturebackend above and app-side renderers (microtube/picoplayer renderernemo.cpp, cameraplus, gstdroid-player).
  No mainline implementation exists in sailfishos-native, dontbeevil, piggz, rinigus or victara -- and none is needed: the software renderer backend is automatic.

## 3. piggz Shutter
- CONFIRMED: https://github.com/piggz/harbour-shutter (main 84220ca 2025-02-26). libcamera direct (CameraProxy: CameraManager, FrameBufferAllocator, Request,
  Stream roles); viewfinder = QQuickItem + QOpenGL shaders over mmap'd buffers (src/viewfindergl.cpp, YUV_2/3_planes, YUV_packed, RGB frags), QML type
  ViewFinderGL (harbour-shutter.cpp:64). Qt6/KF6 since 7bb454e 2024-11-25: Requires libcamera, opencv, qt6-qtsensors, qt6-qtsvg, qt-runner; BuildRequires
  qt6-qtmultimedia-devel, qt6-qtdeclarative-devel, kf6-extra-cmake-modules, kf6-kcoreaddons-devel, qt6-qtshadertools-devel; `%cmake_qt6 -DSAILFISHOS=ON`.
- CONFIRMED OBS: nemo:devel:hw:pine:dontbeevil/harbour-shutter is `unresolvable` on aarch64 (nothing provides qt6-qtmultimedia-devel, qt6-qtdeclarative-devel,
  kf6-extra-cmake-modules, kf6-kcoreaddons-devel; repo paths include only chum:testing 4.6). Only published RPM: Qt5-era
  harbour-shutter-0.0.1+main.20240402143529.395efa3-1.7.6.jolla.aarch64.rpm requiring libcamera.so.0.3 -> does NOT install against native-common
  libcamera-0.4.0+git1 (.so.0.4). Not in chum/chum:testing, no GitHub releases. Deps DO exist in sailfishos:chum:testing 5.1_aarch64: qt6-qtbase/
  qtdeclarative/qtmultimedia/qtshadertools/qtsensors/qtsvg 6.8.4, kf6-kcoreaddons 6.18.0, opencv 4.8.1, qt-runner 0.4.6 (kf6-extra-cmake-modules listed; noarch RPM not checked).
  harbour-advanced-camera (chum 0.9.8) is QtMultimedia Camera+VideoOutput = same stack as jolla-camera, not an independent fallback.

## (a) Least-effort path to a jolla-camera viewfinder (inferred, untested)
1. Replace QT_GSTREAMER_CAMERABIN_SRC=libcamerasrc with `QT_GSTREAMER_CAMERABIN_VIDEOSRC=libcamerasrc` (leave _SRC unset -> wrappercamerabinsrc) in
   /var/lib/environment/nemo/60-multimedia.conf + user-session restart; or stop booster-silica-media@jolla-camera and run /usr/bin/jolla-camera by hand with
   GST_DEBUG=camerabin*:5,wrappercamerabinsrc:5,libcamerasrc:5 QT_DEBUG_PLUGINS=1.
2. Check enumeration: `v4l2-ctl -d /dev/videoN --list-inputs` per node (or QML QtMultimedia.availableCameras). Zero devices -> try first whether
   `gst-launch-1.0 -v autovideosrc ! fakesink` picks libcamerasrc (then no env needed); else rebuild +v4l qtmultimedia with the 5.15 isCamera line.
3. Ignore the droideglsink warning; optionally `rpm -e --nodeps nemo-qtmultimedia-plugins-gstvideotexturebackend`. Still/video capture via
   wrappercamerabinsrc+libcamerasrc (no GstPhotography) is a separate problem; viewfinder first.

## (b) Fallback app path
- Build Shutter for 5.1 aarch64: sfdk target SailfishOS-5.1-aarch64 + repos https://repo.sailfishos.org/obs/nemo:/devel:/hw:/native-common/sailfish_latest_aarch64/
  (libcamera-devel 0.4.0+git1) and https://repo.sailfishos.org/obs/sailfishos:/chum:/testing/5.1_aarch64/ (qt6-*, kf6-*, opencv-devel, qt-runner); or branch
  the dontbeevil OBS package into a home project with those two paths. libcamera 0.4 ABI matches what the device has. Shutter needs no gst sink at all.
- Zero-app sanity check: `gst-launch-1.0 libcamerasrc ! videoconvert ! waylandsink` (gst-plugins-bad waylandsink).
