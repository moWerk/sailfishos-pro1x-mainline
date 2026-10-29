# Boot order audit (2026-10-29)

Five reboots recorded with systemd-analyze (Jolla's `systemd-analyze` package) after the move
to the ABL boot image, plus one boot with a modem crash. Times are seconds since kernel start.

| Stage | Good boots (5) | Crash boot |
|---|---|---|
| initramfs splash / switch_root | 2.4 / 2.8 | same |
| udev coldplug done (udev-settle) | about 7.5 | |
| basic.target | 10.5-11.4 | |
| connman | 10.8-11.7 | |
| ofono (after qx1050-modem-prep) | 14.7-15.4 | 73.9 (prep gave up after 60 s) |
| connman restart (wlan0 appears, stable MAC) | 16.5-17.7 | 99 |
| compositor ready | 22.5-23.0 | |
| graphical.target | 27.0-27.5 | |

Findings and what was done:

- **Modem crash at boot hung the UI.** The modem's watchdog crash ("DOG detects stalled
  initialization", about 1 in 7 boots now, `notes/modem-dog-crash.md`) delays its UIM
  service to 90-100 s. qx1050-modem-prep gave up after 60 s and started ofono on an
  unprepared modem; ofono hung, and lipstick waited on it with a spinner. The script now waits
  up to 300 s, and keeps watching: when UIM disappears and comes back it prepares the modem
  again and restarts ofono. Verified by hand on the hung boot (prep + ofono restart released
  it); the five audit boots had no crash.
- **No ordering cycles** (checked dmesg and the journal; the Jolla tablet's zram loop does not
  exist here: no zram at all).
- **connman is restarted on every boot** at about 17 s by qx1050-wlan-mac (stable MAC).
  Kept on purpose, documented in the unit.
- **Debug units ran on every boot:** qx1050-audio-selftest (a test tone through the speaker,
  plus a suspend/resume of PulseAudio's devices, 25 s after multi-user) and qx1050-blackbox
  (snapshots every 20 s, forever). Both no longer enabled; start by hand when needed.
- **The logs lost the start of every boot:** journald's stock RuntimeMaxUse=1M overflowed
  before the flush to disk, and the kernel ring buffer was flooded by ath10k
  "chan info: invalid frequency" lines. RuntimeMaxUse=16M (journald drop-in) and
  log_buf_len=1M (boot image command line).
- **Slow but not blocking:** systemd-udev-settle (pulled in by usb-moded) holds sysinit.target
  until about 7.5 s; mce takes 4.7 s (waiting for the charger driver, drop-in), the sound card
  reprobe 8.2 s, init-done 17 s.

Open: the 20 s spinner between lipstick and the lock screen that the author saw on some boots
did not show in these five; it needs a recording of such a boot. The ath10k log spam.
