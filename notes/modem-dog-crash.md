# Modem watchdog crash at boot ("DOG detects stalled initialization")

Status 2026-10-28: unsolved, intermittent, recovers by itself (the modem restarts ~40 s
later), but delays Wi-Fi (wlan0 comes through the modem's WLAN service), mobile data and
the SIM by about a minute.

## What was measured (SailfishOS 5.2, kernel 7.3)

- Frequency: 4 of 7 logged boots, then 3 of 4 in a series, 4 of 4 (one twice) with the
  modem start delayed to 30 s, 1 of 3 with qx1050-modem-prep (QMI polling) disabled.
- Timeline of a crash: rmtfs starts the modem (`-s`) 20-30 ms after it is up itself, the
  modem reports "running", then nothing until the watchdog 40.0 s later. With rmtfs `-v`
  the modem makes **no rmtfs request before the crash**: the first entry is its "bye",
  and only the recovered modem opens /boot/modem_fs1 and modem_fs2. It stalls before EFS.
- Not the cause: rmtfs start order (identical in clean and crash boots), the in-kernel PD
  mapper (built in), the IPA driver (same "starting/running" events in both), an early
  start (delaying to 30 s made it worse), our QMI polling alone (1 of 3 without it).
- NixOS (kernel 7.1, ModemManager): the modem starts at ~51 s and did not crash in the
  boots checked past the critical point (2 boots). Too few samples to call it clean.
- tqftpserv rejects mbn_hw.dig / mbn_sw.dig (modem configuration files Linux firmware
  sets lack) in every boot; the requests come ~10 s after a successful start, so after
  the point where crash boots stall.

## Next ideas

- Compare the SailfishOS and NixOS kernels' modem-side differences (7.3 vs 7.1: remoteproc,
  qcom_q6v5_pas, sysmon, glink, the IPA driver).
- More NixOS boots past 100 s uptime to establish whether it is really clean there.
- What else talks to the modem or QRTR early on SailfishOS (hexagonrpcd and sensorfwd query
  QRTR for the ADSP; connman; ofono is started late since a24b9e4).
