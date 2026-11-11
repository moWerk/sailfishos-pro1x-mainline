# Fingerprint on the mainline Pro1-X

State 2026-11-11, 01:50: **works from the stock UI.** A finger is enrolled in Settings > Device
lock (14 touches) and unlocks the phone on the lock screen after a sleep; two fingers were
enrolled and both unlock (author). Kernel kfp2, see the series below.

## What the sensor is

A MicroArray capacitive sensor (chip id 0x24) on QUP SE5 SPI. TrustZone owns that bus: the
SPI controller is disabled in every stock device tree and GPIO 14 to 17 are reserved on every
SM6115 phone in mainline. No Linux SPI driver can drive it. The stock Android stack is the
trusted application `mafp.mbn` (TZ name `fingerprint`, a clone of Qualcomm's `smplap32`
sample app), the MicroArray client library `libfprint-x64.so` (unrelated to freedesktop
libfprint) and a kernel driver `madev` that only switches power (TLMM 108) and forwards the
sensor interrupt (TLMM 97). Everything that reads the sensor is inside the TA.

## The stack on mainline

| layer | what | where |
|---|---|---|
| kernel | Bartosz Wróbel's out-of-tree QSEECOM TEE driver (`drivers/tee/qseecom`, the QSEECOM listener part of `qcom_scm.c`), plus three fixes | `kernel/patches-fp/` |
| supplicant | `qsee-supplicant` (FS listener 10, GPFS listener 28672; templates land under `/var/lib/qsee-supplicant/data/system/users/0/fpdata/`) and `qsee-app-loader@mafp`, which keeps the TA loaded | `packages/qsee-supplicant/` |
| firmware | the stock `mafp.mbn` alone in `/lib/firmware` (with the `.mdt` beside it the driver wants a `.b07` the stock image does not have); not redistributable, not in this repository | the stock vendor image |
| daemon | `sailfish-fpd-mafp`: `org.sailfishos.fingerprint1` (the API of Jolla's `sailfish-fpd`, modelled on sailfish-fpd-community's D-Bus side) over the TA protocol; holds TLMM 108 high; polls the TA's touch detection every 30 ms; `Provides: sailfish-fpd` | `packages/sailfish-fpd-mafp/` |
| device lock | Jolla's `sailfish-devicelock-fpd` with `sailfish-devicelock-libs` from the release repository. It replaces `jolla-devicelock-daemon-encsfa` and `-plugin-encsfa` (`zypper in --force-resolution`), ships the `encsfa-fpd` device lock plugin and makes Settings show the Fingerprint section (`FingerprintSensor.hasSensor`) | Jolla repo |

Order of a session, as the Android library does it: keymaster64 command 0x205 gives the auth
token key; mafp 0xC8 init with that key (the first command must be this one, any other first
command kills the instance), 0xEF chip id, 0xC9 service init with the chip-0x24 configuration
defaults, 0xCA set active group with the store path. Enrolment: 0xD8/0xD9, 0xCD, 0xCE, then per
touch 0xD2 mode 1 (poll), 0xD3, 0xCF, lift; 0xD0, 0xE2. Verify: 0xE1, 0xDA, 0xE2, 0xD6, then
per touch 0xD2 mode 0, 0xD2 mode 4 (the capture, up to five times), 0xD5; reply word 1 is the
matched template id, 0 for none. The TA answers in place, over its request: the daemon passes
the request as an INOUT memref and the driver (fix 0004) copies it back. The full command
table with offsets is in kabouik's `device/fingerprint/README.md` of his Guix port and in the
daemon's source (`src/mafpta.h`).

## The three hard hangs of 2026-11-09, explained

Two kernel bugs, stacked:

1. The driver's listener loop gave up on a command after 256 served rounds. The TA reads its
   template store through the FS listener in 20 KB requests: 49 per 968704-byte slot file.
   With five slots on file, set active group runs past 256. The loop told TZ the request
   failed and the TA errored out (`qseecom: listener 10 will not settle, failing it`,
   `scm call failed with error -40`). With zero or one slot the same command had passed
   about twenty times, which is why the morning went well and the afternoon hung.
2. The next command asked TZ for an SHM bridge and got firmware status 30. Mainline's
   `qcom_scm_shm_bridge_create()` returns that positive status; `qcom_tzmem_shm_bridge_create()`
   passes it up although it documents a negative return, `qcom_tzmem_pool_new()` wraps it in
   `ERR_PTR()`, `IS_ERR()` does not recognise a small positive value, and `qcom_tzmem_alloc()`
   dereferenced the integer 30: `Unable to handle kernel NULL pointer dereference at virtual
   address 000000000000001e`, x19 = 0x1e. That is an upstream bug (since the commit that
   enabled SHM bridge support in 2024), met by any driver that creates a tzmem pool per command.

Fixes, `kernel/patches-fp/`: 0002 maps the firmware status to -EIO at the tzmem helper
(upstream candidate); 0003 counts consecutive unserved listener rounds (cap 16) instead of
served ones and still answers TZ before giving up; 0004 copies the request back when the client
marks it INOUT. With them, set active group with five slots on file ran clean, and nothing in
the kernel log since.

**Do not boot a kernel without 0002 and 0003 while `sailfish-fpd-mafp` is enabled**: its first
command is set active group and the hang comes back on every boot.

## Lessons

- Device lock clients (jolla-settings, lipstick) keep their socket to the device lock daemon.
  After installing `sailfish-devicelock-fpd` nothing shows until they are restarted; a reboot
  is simplest. The device lock daemon had read our finger list long before.
- The first boot asks for the lock code before the sensor is used, as on Jolla's devices.
- Matching in the first tests: about one touch in three matched, the misses return template
  id 0 with status 0. Placement and rhythm are the first suspects; the capture step
  (mode 4) the second. Enrolment through the UI and the by-hand enrolment both match.
- Once (after a clear of all templates and an enrolment) the TA instance died: TZ refused
  every command with -22. Recovery: restart `qsee-app-loader@mafp.service` (reloads the TA)
  and `sailfish-fpd-mafp.service`. The daemon does not recover on its own yet.
- The USB gadget dies after the first suspends (`HS-PHY not in L2`, known, a reboot restores
  it), and the phone answers Wi-Fi only while the screen is on. Work with the screen on, or
  over USB right after a boot.

## Open

Match rate; the sensor interrupt (TLMM 97) instead of polling; sensor power stays on while
the daemon runs; automatic recovery of a dead TA instance; folding the fp series into the
daily kernel; the auth token path (the TA accepts a zero token at enrol and verify; what a
signed token would be good for on Sailfish is nothing yet); the kernel changes to Wróbel and
the tzmem fix upstream (both wait for the author's read-back).

## Credits

Bartosz Wróbel (the QSEECOM TEE driver and qsee-supplicant), kabouik (the first mapping of the
mafp commands from the blobs and the parallel work on his Guix port), the sailfish-fpd-community
contributors (the D-Bus side the daemon follows), nyymi609 (the NixOS base both ports grew on).
The daemon, the protocol work and the kernel fixes were made with LLM assistance; the author
enrolled and tested on the device.
