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

## State 2026-10-26: step 1 done, step 2 prepared

- Built with mb2 (port/packages/, upstream trees copied in like qrtr):
  - `protobuf-c` 1.5.2 (not in the SFOS repos; builds against Jolla's protobuf 25.1).
  - `libssc` 0.4.4 + `libssc-tools` (`ssccli`). The mock SSC server and the tests are
    switched off (rpm/0001-…: they need python3-embed).
  - `hexagonrpc` 0.5.0+git40e5041: PR 27 head + nyymi609's sns_registry patch, both applied
    in the tree. The SDK kernel headers lack `misc/fastrpc.h`, so the v6.18 UAPI header is
    in `include/`. Upstream units dropped (they install to lib64/systemd and lack `-R`).
- libqmi 1.34 from native-common is built with QRTR support (`qmi_qrtr_supported=1`).
- Adaptation (droid-config-qx1050 sparse):
  - `/usr/share/qx1050/hexagonfs`: the data from fxtec-pro1x-nixos device/hexagonfs
    (collected from nyymi609's unit; the registry holds default placement/calibration, so a
    per-unit dump with sensors-collect.sh may be wanted later).
  - `/usr/bin/droid/qx1050-hexagonrpcd-prepare.sh`: port of the NixOS ExecStartPre.
  - `qx1050-hexagonrpcd.service`: **not enabled**.
- On pro1x2 (SFOS): the four RPMs and the files are installed. Nothing has been started, and
  the ADSP recovery setting is untouched.
- Observation (read-only `qrtr-lookup`): the ADSP (node 5) already announces service 400,
  "Snapdragon Sensor Core", with no hexagonrpcd running. So the service exists before the
  registry is served. Whether sensors answer without the registry is unknown; do not probe
  it without the author present.
- sensorfw-qt5-devel (jolla repo) ships `deviceadaptor.h`, `iioadaptor.h`, `sysfsadaptor.h`
  and `libsensorfw-qt5.so`. The adaptor can therefore be an out-of-tree plugin RPM, with no
  sensorfw fork.

### First run on SailfishOS (author present, ~10 minutes)

1. Re-arm the SFOS one-shot first (`DATA=/mnt/nixos sh /root/sfos-boot-once.sh`). A
   crash-reboot then lands in SFOS again instead of NixOS.
2. Terminal A: `journalctl -f`. Terminal B: `dmesg -w | grep -i -e adsp -e fastrpc -e remoteproc`.
3. `systemctl start qx1050-hexagonrpcd` (the prepare step sets the adsp remoteproc to
   `recovery=disabled`). Expected: the daemon stays up, and the ADSP logs registry reads.
4. `ssccli --sensor light`, then `proximity` (cover the top of the screen), then
   `accelerometer` (turn the phone), `magnetometer`, `compass`, each with `--timeout 10`.
5. Failure modes:
   - "adsp crashed" in dmesg: audio is gone until reboot (the reprobe workaround cannot
     help). Collect the journal, then reboot.
   - A hang: hold power for 10 s. Step 1 brings it back to SFOS.
6. When 4 prints samples, the next step is the sensorfw adaptor (step 3 above). Enable the
   unit only after several clean boots with a manual start.

## Why it was not built on 2026-10-24 (and the correction)

The SDK target image used for the rootfs (`localhost/sfos-target-aarch64:5.1.0.11`) has
rpmbuild and qmake but no compiler: the Sailfish SDK keeps the toolchain in its x86_64
"tooling" and enters the target through scratchbox2. The night session took that for a
blocker. Correction (2026-10-24 afternoon): the Platform SDK with that tooling and the
aarch64 target has been running on this host since 2026-10-04 in the rootless podman
container `sfos-sdk-setup` (`~/sailfish/scripts/sfos-mb2-build.sh <source dir>`, see
`~/Git/llm-rag/SailfishOS/sdk_on_rootless_podman.json`); it built mce then and the patched
qtmultimedia for this port now. Steps 1 to 3 are buildable with it (five small specs via mb2;
the same thing OBS would do, nemo:devel:hw:fxtec:qx1050 exists but is empty, see
notes/research/research-packaging.md).

Estimated effort once a build path exists: packaging and step 2 in a day, the adaptor in a
few days including on-device calibration of axes and the proximity threshold.

## Update 2026-10-27: Affe Null's sensorfw IIO refactor

Michał pointed at github.com/affenull2345/sensorfw commit 945e037 (WIP, 2025-12-13, not in
sailfishos/sensorfw): a `core/iioadaptor` base class (finds the IIO device, reads the IIO
buffer, calls `processSample(const qreal *data, quint64 timestamp)`) and one small adaptor
plugin per sensor (accelerometer, gyroscope, magnetometer, als, proximity, orientation) that
only maps channel values to sensorfw data.

It does not read our sensors (SSC, not IIO), but it changes step 3 of the design: instead of
one `sscadaptor` registering under five names, an `SscAdaptor` base class with the same
`processSample(data, timestamp)` contract (libssc GLib loop in a worker thread, one libssc
sensor object per instance), and per-sensor plugins shaped like his. Then the per-sensor half
could be shared with his IIO adaptors if the transport is split out (a point to raise with him
before writing it, so two transports do not grow two copies of the same sensor code).

First run (step 2) is still pending with the author present: the prepare step disables ADSP
recovery, so an ADSP crash would freeze the phone (remoteproc recovery is enabled on all three
DSPs on 5.2 as of today).
