# sailfish-fpd-mafp: the fingerprint daemon for the Pro1-X on mainline

Implements `org.sailfishos.fingerprint1`, the D-Bus API of Jolla's `sailfish-fpd`, on top
of the Pro1-X's MicroArray sensor. The sensor hangs off a TrustZone-owned SPI bus and is
driven by the stock `mafp` trusted application (TZ name `fingerprint`), which the daemon
talks to through Bartosz Wróbel's out-of-tree qseecomtee driver and qsee-supplicant. No
libhybris, no Android HAL.

**State (2026-11-11): written, not yet run.** Everything the daemon does was done by hand on
2026-11-09 with the test tool (`mafp-init`): application init, chip id 0x24, template store
through the supplicant, enrolment of a finger (14 samples). Verify never matched then (six
attempts, template id 0 back); the daemon sends what the Android library sends as far as it
is mapped, and Identify is expected to report misses until that is understood. The three
hard hangs of that day were two stacked kernel bugs, fixed in kernel kfp2
(`reports/pro1x-fingerprint-hang-rootcause-2026-11-11.md` in the memeline session's reports);
the daemon must not run on kernel kfp.

## Layout

| file | what |
|---|---|
| `src/teeclient.*` | the TEE ioctls: device by implementation id, shared memory, session by name, invoke with the request as INOUT (the driver copies it back; the application answers in place) |
| `src/mafpta.*` | the mafp protocol: keymaster64 auth-token key, init, chip id, service init (`g_config` defaults for chip 0x24), active group, detect_touch, enumerate, remove |
| `src/sensorpower.*` | TLMM GPIO 108 (`500000.pinctrl`) held high through the GPIO character device for the daemon's lifetime |
| `src/mafpbackend.*` | enroll, authenticate, enumerate, remove on a worker thread; the Android HAL's sequences; results as Qt signals |
| `src/fpd.*` | the D-Bus service, adapted from sailfish-fpd-community (GPL-3.0, sailfishos-open): states, reply codes, the name map in `/var/lib/sailfish-fpd-mafp/fingerprints.db` |

## On the device

- Kernel with `CONFIG_TEE_QSEECOM=m` and the three fixes (kfp2).
- `qsee-supplicant` (packaged beside this), `qsee-app-loader@mafp.service`, `/lib/firmware/mafp.mbn`
  from the stock image.
- `sailfish-devicelock-fpd` from the Jolla repository (replaces `jolla-devicelock-*-encsfa`;
  it is what makes Settings > Device lock show the Fingerprint section through
  `FingerprintSensor.hasSensor`).
- This package provides `sailfish-fpd`, so nothing pulls Jolla's `sailfish-fpd`, which needs
  the closed hybris slave.

The sensor's finger wait polls `detect_touch` every 30 ms; the sensor interrupt (TLMM 97) is
not used yet. Sensor power stays on while the daemon runs.

## Credits

The D-Bus side follows sailfish-fpd-community by the sailfishos-open contributors, who
reverse engineered the API from sailfish-fpd and mce. The qseecomtee driver and
qsee-supplicant are Dawid Wróbel's. kabouik mapped the mafp command table from the blobs
first, for his Guix port.
