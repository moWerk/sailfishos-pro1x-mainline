# Modem watchdog crash at boot ("DOG detects stalled initialization")

Status 2026-10-30: **cause found, fix not yet applied.** A race in the QRTR HELLO
handshake that Linux 7.3 introduced: when it hits, the kernel name service never tells the
modem about the AP's services (rmtfs among them), the modem waits for them and its watchdog
fires 40 s later. It recovers by itself on a later start, but every failed start delays
Wi-Fi (wlan0 comes through the modem's WLAN service), mobile data and the SIM by 40 s.

## Cause (2026-10-30)

Journal of 25 boots (2026-10-29/30), every modem start compared:

- **Every** start followed within 3 ms by the kernel line `failed to announce new service`
  (with `failed while handling packet from 0:-2`, the control port) crashed exactly
  40.03 s later; **no** start without it crashed. 25 of 25 boots, including boots with two
  and three failed starts in a row and their clean last start.
- Everything else is identical between good and bad boots to the millisecond: rmtfs,
  tqftpserv, hexagonrpcd, sensorfwd, ofono and modem-prep all start 0.2-0.8 s before the
  modem is up. In good boots ath10k (WCN3950 through the modem) appears 1.2 s after the
  modem; in bad boots only after the recovered start.
- The line comes from `announce_servers()` in `net/qrtr/ns.c`: on the modem's HELLO the
  name service announces every AP-side server to it and **aborts on the first send error
  other than -ENODEV**, so the rest are never announced. That matches the 2026-10-28
  rmtfs `-v` finding below: a crashing modem never asks rmtfs for anything.
- The send error is -EAGAIN from the gate that 7.3 added in `qrtr_node_enqueue()`: until
  the AP's own HELLO to that node went out (`hello_sent`), every other packet returns
  -EAGAIN, and a HELLO that itself got -EAGAIN is retried by delayed work 100 ms later. If
  the modem's HELLO reaches the name service before ours is out, the announcement fails
  and nothing repeats it. (Read from the source; the errno itself was not traced.) The
  gate came with "net: qrtr: Send HELLO message on endpoint register" (Chris Lew, v3
  2026-08-27, merged into net as 544d85de4dc2 on 2026-09-02,
  https://ratatoskr.run/lkml/2026/08/17469420/t). The NixOS kernel (7.1) predates it,
  which fits the clean NixOS boots.
- Another SM6115 mainline port hit the same: Lenovo TB-J606L on 7.3, "DOG detects stalled
  initialization" at about every second cold boot, only 2 of 42 QRTR services visible
  during the stall, Wi-Fi 40 s late. They fixed it by keeping the 7.2 handshake
  (https://github.com/haltia-de/device-lenovo-tb-j606l/issues/31, commit 3768c60, patch
  `0020-net-qrtr-keep-the-hello-handshake-of-7.2.patch`: drop the `hello_sent` gate, the
  name service answers every remote HELLO again; five clean boots in a row). As of
  2026-10-28 no fix upstream (net, net-next, linux-next). They saw it with PSCI idle on,
  and `cpuidle.off=1` hid it; idle timing only moves the race. PSCI idle is on here too
  (`psci_idle`, cpu-sleep-0-0 in use).

## Experiments for daytime (each needs reboots; mo decides)

1. **Kernel patch: keep the 7.2 HELLO handshake** (as J606L 0020), then 10+ boots counting
   `failed to announce` and crashes. Expected: none. The smallest change that removes the
   race.
2. Alternative patch, closer to upstream's intent: keep the gate but let the name service
   repeat `announce_servers()` for that node after -EAGAIN (or announce once `hello_sent`
   becomes true). Better for an upstream report; needs more care.
3. Diagnostic only, no rebuild: one boot image with `cpuidle.off=1` on the cmdline, to
   confirm the timing dependence here. Costs idle power, not a fix.
4. Report upstream (netdev, linux-arm-msm, Chris Lew) with the evidence from two SM6115
   devices, after mo's read-back.

Counting tool: `journalctl -b <n> -k | grep -c "failed to announce new service"` against
`grep -c "handling crash"`.

## What was measured before (SailfishOS 5.2, kernel 7.3, 2026-10-28)

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

## Ideas from 2026-10-28 (superseded by the cause above)

- Compare the SailfishOS and NixOS kernels' modem-side differences (7.3 vs 7.1: remoteproc,
  qcom_q6v5_pas, sysmon, glink, the IPA driver).
- More NixOS boots past 100 s uptime to establish whether it is really clean there.
- What else talks to the modem or QRTR early on SailfishOS (hexagonrpcd and sensorfwd query
  QRTR for the ADSP; connman; ofono is started late since a24b9e4).
