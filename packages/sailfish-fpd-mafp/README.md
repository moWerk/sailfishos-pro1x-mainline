# sailfish-fpd-mafp: the fingerprint daemon for the Pro1-X on mainline

Implements `org.sailfishos.fingerprint1`, the D-Bus API of Jolla's `sailfish-fpd`, on top
of the Pro1-X's MicroArray sensor. The sensor hangs off a TrustZone-owned SPI bus and is
driven by the stock `mafp` trusted application (TZ name `fingerprint`), which the daemon
talks to through Bartosz Wróbel's out-of-tree qseecomtee driver and qsee-supplicant. No
libhybris, no Android HAL.

**State (2026-11-11, kernel kfp2): working.** The daemon came up from boot with the supplicant
and the loader, enumerated the templates, and the author enrolled a finger from the stock
Settings page (15 touches, one rejected by the TA with its code -2988) and unlocked the phone
with it on the lock screen after a sleep. Identify matched both the finger enrolled by hand on
2026-11-09 and the one enrolled from Settings. Observed in the first tests: about one touch in
three matched, the rest came back as no match with status 0 (placement, rhythm or the capture
step; not understood yet). Monday's "verify never matched" was tests against empty slots and a
lock screen Identify before any finger of the author was on file.

Two integration lessons: the device lock clients hold their connection to the device lock
daemon, so after installing `sailfish-devicelock-fpd` the Settings app and lipstick must be
restarted (a reboot is simplest) before the Fingerprint section and the lock screen Identify
appear; and the first boot asks for the lock code before the sensor is used, as on Jolla's
devices.

Known problem (2026-11-11, once): after a Clear of all templates and an enrolment, the TA
instance died; from the next command on TrustZone refused everything with -22 (daemon log
`UNABLE_TO_PROCESS`, Settings "could not be captured"). Recovery: `systemctl restart
qsee-app-loader@mafp.service` (reloads the TA) and `systemctl restart sailfish-fpd-mafp.service`;
after that two fingers enrolled and both unlock. The daemon does not yet detect a dead instance
and recover on its own: next change.

The three hard hangs of 2026-11-09 were two stacked kernel bugs, fixed in `kernel/patches-fp/`
0002 and 0003 (`notes/fingerprint.md`); the daemon must not run
on kernel kfp.

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
