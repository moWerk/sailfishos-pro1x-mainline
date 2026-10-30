# System sleep (suspend) on the Pro1-X

Status 2026-10-30 15:00: **s2idle works, the SoC never reaches deep sleep, mce wakes it about every minute.**

## What works (kernel out-7.3-f, 2026-10-30)

- `CONFIG_PM_AUTOSLEEP=y`: mce suspends through `/sys/power/autosleep` (`freeze`). Before
  that the phone never suspended at all (0 in `suspend_stats` after hours screen-off).
- The ADSP sensor process crashed at the first resume (`EX:sensor_process:0x1:frpc_dsp:0xbe`,
  ADSP recovery is off, so no sensors and no audio until reboot). Fixed with the TB-J606L
  port's patches 0032-0034 (fastrpc resumes an interrupted invocation, fastrpc wakeup event per
  DSP message, SMP2P sleepstate driver + DT): 39 suspends without an ADSP crash.
- Wakes: power key (about 0.5 s slower than from blank-only), keyboard slide, RTC alarms.
  Double-tap did not wake from suspend: the goodix driver put the controller to sleep; kernel
  patch 0035 keeps it scanning as a wakeup source (built, `out-7.3-g`).
- Wi-Fi deauthenticates before each suspend and reconnects about 5 s after resume (no
  WoWLAN), so ssh is not possible while the phone sleeps.

## What does not: vlow/vmin 0

`/sys/kernel/debug/qcom_stats/` (module `qcom_stats`): vlow Count 0, vmin Count 0 after 39
suspends; vlow "Client Votes: 0x8f878f87". The SoC stays in shallow s2idle with XO and rails
up. The TB-J606L port sees the same (also after adding MPM).

Research summary (2026-10-30, sources in the session log):
- vlow = XO shutdown, vmin = XO off plus CX/MX retention; the RPM enters them only when all
  masters (APSS, MPSS, ADSP, CDSP, TZ) sleep and the sleep-set votes allow XO off. The APSS
  reports its sleep through the MPM, which mainline sm6115.dtsi lacks (agatti.dtsi has it; the
  J606L port added it plus a late psci_idle_init so the CPUs leave WFI-only idle).
- clk-smd-rpm's handoff votes INT_MAX into both active and sleep sets for every clock; unclaimed
  clocks then block XO shutdown (fixed for QCM2290 only, not in master).
- sm6115.dtsi interconnect paths are mostly RPM_ALWAYS_TAG (vote in the sleep set too).
- `qcom,rpm-master-stats` (downstream bengal: 0x45f0150, masters APSS MPSS ADSP CDSP TZ,
  offset 4096) would show which master stays awake; mainline sm6115 has no node.
- A working SMD-RPM template: the TicWatch Pro 3 (msm8937) port reached XO shutdown and vmin
  with MPM + RPM wakeup timer, dropping unclaimed clock votes at sync_state, XO from xo_board,
  Venus off, i2c/spi clock leak fixes.

Next on the device: cpuidle states and rejects, pm_genpd cluster S1 usage/rejected, clk_summary
holders of bi_tcxo/xo, interconnect sleep votes; then MPM + rpm-master-stats in the DT.

## First measurements (2026-10-30)

- 13:34-14:06: not a sleep window: msyncd (account sync) started a CalDAV/CardDAV sync right after
  a resume while Wi-Fi was still down, failed with HostNotFound and kept its mce cpu keepalive
  ("long keepalive active") for 30 min. Masked for the next window.
- 14:16-14:55 (msyncd masked): the gauge log shows a wake every ~55 s lasting a few seconds at
  230-390 mA; RTC wakeups 31 this boot, mce_rtc_wakeup 36 events, dsme 3. 16 suspend attempts
  aborted with "tasks refusing to freeze" after 2-4 ms (a wake event during freeze). The author
  read about 27 % before and 25 % after on the display: about 2 % in 39 min, roughly 100 mA on
  average; the driver's own figures jumped (see the gauge note below).
- Awake with the screen off: 230-340 mA.
- Gauge: the driver integrates current only while awake and pulls the count to its OCV estimate
  after gaps; a 30 s charger connection before sleeping raised the estimate by about 12 % (charging
  polarisation read as charge). Fixes due: ignore the OCV estimate for a while after unplugging,
  and read the QG's sleep samples after resume as the downstream qpnp-qg does.
- Since 14:59 wanze (a-d-b-wanze: one sample per awake moment, timer never wakes the phone)
  logs to /var/log/wanze.csv with the wakeup source column on.
