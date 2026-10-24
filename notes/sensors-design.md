# Sensors on the Pro1-X mainline port: what exists, what is missing, how to build it

State 2026-10-24: no sensor works on SailfishOS (no auto brightness, no proximity, no
rotation). This note is the design for the missing piece and the reason it was not built in
the first bring-up sessions.

## Facts (from notes/research/research-sensors-camera.md, confirmed)

- SM6115 has no plain IIO sensors. Accelerometer, gyroscope, magnetometer, light and
  proximity sit behind the Sensor Subsystem (SSC) on the ADSP and are reached over FastRPC
  plus QMI/QRTR; the only IIO devices are the PMIC ADCs.
- NixOS gets them with hexagonrpcd (linux-msm/hexagonrpc, PR 27 head plus two patches of
  nyymi609: `packages/hexagonrpc-*.patch`) and iio-sensor-proxy 3.9 with libssc 0.4.4
  (codeberg DylanVanAssche). libssc is a GLib library (depends on libqmi-glib >= 1.33.4,
  libqrtr-glib, protobuf-c) with per-sensor APIs; it creates no kernel device.
- SailfishOS reads sensors through sensorfw (sensorfw-qt5 0.15.1 on 5.1): adaptor plugins
  (`lib<name>-qt5.so`) selected per sensor in `/etc/sensorfw/primaryuse.conf`. Upstream
  adaptors: iio, evdev/sysfs variants, hybris. Nothing for SSC/libssc, and nothing for
  iio-sensor-proxy's D-Bus (which would not carry raw accelerometer/gyro/magnetometer data
  anyway: net.hadess.SensorProxy exposes only orientation, light, proximity, compass).
- The ADSP sensor service also needs the registry/calibration blobs nyymi's firmware package
  carries (hexagonfs data) and a udev rule giving the daemon the fastrpc device.
- Known hazard (NixOS README): an ADSP crash with recovery disabled freezes the phone; with
  recovery enabled audio restarts. Any daemon talking to the SSC must be tested with that.

## Design

1. Packages (RPM, aarch64, built against SFOS 5.1): `libqmi`/`libqrtr-glib` (native-common
   already has them), `protobuf-c`, `libssc`, `hexagonrpcd` (fork at PR 27 + nyymi's two
   patches, with its systemd unit and the hexagonfs data path), udev rule for
   `/dev/fastrpc-adsp` (group `sensors` or root-only since sensorfw runs as root).
2. Verify the daemon alone on SailfishOS: `hexagonrpcd` up, `qrtr-lookup` shows the sensor
   service (SNS client QMI service 0x190), a small libssc test client reads accelerometer
   samples. This step needs no sensorfw and proves the firmware/registry side.
3. sensorfw adaptor `libsscadaptor-qt5.so`: one plugin class registering as accelerometer,
   gyroscope, magnetometer, als and proximity adaptor (the iio adaptor is the model: one
   class, several adaptor names), running libssc's GLib main loop in a worker thread and
   pushing samples into sensorfw's ring buffers with the transformation matrix
   `-1,0,0, 0,-1,0, 0,0,1` (NixOS ACCEL_MOUNT_MATRIX, to be verified on the device).
   Config: `primaryuse.conf` mapping the five sensors to `sscadaptor`, a
   `sensord.conf.d/90-qx1050.conf` with intervals, and the csd `hw-settings.ini` feature
   flags so Settings shows the sensor items.
4. mce then gets ALS and proximity through sensorfw as on every other device (double-tap
   wake and the kbd backlight ALS profile are already configured for it).

## Why it was not built on 2026-10-24

The SDK target image used for the rootfs (`localhost/sfos-target-aarch64:5.1.0.11`) has
rpmbuild and qmake but no compiler (`gcc`, `g++`, `make` missing): the Sailfish SDK keeps
the toolchain in its x86_64 "tooling" and enters the target through scratchbox2/sfdk, which
is not set up on this host (the SDK-on-rootless-podman work is a separate, open item, see
`~/Git/llm-rag/SailfishOS/sdk_on_rootless_podman.json`). The nix cross toolchain can build
static C tools (used for `tools/ffrumble.c`) but not a Qt plugin against SailfishOS's
sensorfw headers and ABI. So steps 1 to 3 need either the Platform SDK with the aarch64
target (mb2 build of five small specs; the proper way, also what OBS would do) or a
community OBS project (nemo:devel:hw:fxtec:qx1050 exists but is empty, see
notes/research/research-packaging.md).

Estimated effort once a build path exists: packaging and step 2 in a day, the adaptor in a
few days including on-device calibration of axes and the proximity threshold.
