# libcamera 0.7.2 for the Pro1-X port

Packaging from mlehtima/libcamera (the source of nemo:devel:hw:native-common's 0.4.0+git1),
pointed at upstream v0.7.2, because 0.4.0 asserts in `PipelineHandler::stop()` when a camera
is released (software-ISP stop fixes landed in 0.5.0, request completion on stop in 0.6.0).

Build tree = `git archive` of upstream v0.7.2 with this `rpm/` directory next to `meson.build`
(mb2 builds in tree and skips %prep, so `0001-…compat…patch` is also applied directly in the
tree; the spec lists it for an OBS build). Changes against native-common's spec: qcam,
lc-compliance, tests, SDL2 output, libunwind/libdw off; pipelines limited to simple + uvcvideo
(the virtual pipeline would need libyuv); `libevent-devel`/`libv4l-devel` named directly
(mb2's resolver did not see the pkgconfig capabilities from the added native-common repo).
The compat patch defines clone3/pidfd bits missing from the SDK's 4.4 kernel headers.
