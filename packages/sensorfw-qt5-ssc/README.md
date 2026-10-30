# sensorfw-qt5-ssc: sensord adaptors for the Snapdragon Sensor Core

Five out-of-tree sensord plugins (`libssc{accelerometer,gyroscope,magnetometer,als,proximity}adaptor-qt5.so`)
that read the SSC on the ADSP through libssc 0.4.4. Built against sensorfw-qt5-devel 0.15.1 (no
sensorfw fork). Step 3 of `notes/sensors-design.md`.

- `common/sscadaptor.*`: `SscAdaptor`, a `DeviceAdaptor` with HybrisAdaptor's running/standby
  bookkeeping and a small state machine (create -> open <-> close) over libssc's async API.
  libssc is GLib based; sensord's Qt loop runs on the GLib default context, so no thread.
- One plugin per sensor, shaped like the hybris adaptors (and like Affe Null's per-sensor IIO
  adaptors): convert libssc's `measurement` signal to sensorfw units (m/s^2 -> milli-G,
  rad/s -> milli-degrees/s, uT -> nT, lux, near/far).
- `config/60-qx1050-ssc.conf`: the `[plugins]` mapping and the NixOS mount matrix. **Not
  installed** by the package: mce starts ALS and proximity as soon as sensord loads it, which
  must wait for the first supervised run with hexagonrpcd.

Build: the SDK target needs our libssc, libssc-devel, protobuf-c(-devel) RPMs installed by hand
(not in any repo); then `sfos-mb2-build.sh packages/sensorfw-qt5-ssc`. 2026-10-27: builds without
warnings; installed on pro1x2 (inert without the config). Never run on hardware yet.

Known gap: an adaptor destroyed while an async libssc call is pending gets the callback with
a dangling pointer (sensord only destroys adaptors at exit).

First run (with the author), after `ssccli` has shown samples:
1. `cp /root/rpms/60-qx1050-ssc.conf /etc/sensorfw/sensord.conf.d/; systemctl restart sensorfwd`
2. `journalctl -u sensorfwd -f`: "registering ssc...adaptor", then start/stop lines from mce.
3. Settings: auto brightness, proximity during a call, rotation; check the axes.

## Step counter (2026-10-30)

`sscstepcounteradaptor` registers `stepcounteradaptor` for sensorfw's stepcountersensor, fed by
libssc's pedometer (libssc patch 0003). The config enables the sensor (`[available]`), which
20-sensors-default.conf hides. ssccli measured 181 steps for 170 walked (pedometer verified);
the adaptor loads (`loadPlugin stepcountersensor` true) but has had no client yet: sensortestapp
predates the step counter and QtSensors 5.6 on SailfishOS has no QStepCounterSensor, so a client
reads it through libsensorclient's StepCounterSensorChannelInterface. The SSC counts only while
a client keeps it enabled; counting through AP suspend needs such a client to stay open (the
asteroid-sensorlogd model). Not yet tested with the AP suspended: the GNSS test held a wakelock.
