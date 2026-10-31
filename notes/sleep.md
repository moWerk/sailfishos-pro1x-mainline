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

## The minute wake: connman, Wi-Fi without WoWLAN (2026-10-30 evening)

- Kernel timestamps: a wake at second :11-:12 of every minute, suspend again 5 s later. dsme's
  iphb (debug: `dsmetool -l 7 -i "iphb*:*"`) names the client: connmand asks for a wakeup every
  54-59 s with the wakeup flag; mce only while the display is on, dsme's own clients do not wake.
- Wi-Fi had no Wake-on-WLAN, so cfg80211 disconnected before every suspend ("deauthenticating
  ... by local choice") and connman kept reconnecting and scanning. ath10k on the WCN3950
  offers WoWLAN (disconnect, magic packet, patterns, net detect); `70-qx1050-wowlan.rules`
  now enables disconnect + magic-packet. The association survives suspend and sleeps of
  3-12 minutes appear.
- What is left of the rhythm comes from connman's sailfish_wifi autoscan: every autoscan arms
  a 5 s hold-off as a wakeup timer (`connman_wakeup_timer_add_seconds(WIFI_AUTOSCAN_MIN_SEC)`),
  the phone suspends after 5 s, and iphb's 30 s grain turns the hold-off into a wake about a
  minute later, which can start the next scan. A hold-off has no reason to wake the system:
  a plain g_timeout would do (connman patch, not done). The 300 s autoscan itself also wakes.
- The FritzBox drops the sleeping phone every 10-15 min (Reason 7 CLASS3_FRAME_FROM_NONASSOC_STA,
  once Reason 6): the AP times the station out, the firmware apparently sends no keepalive in
  WoW. Each drop wakes the phone (disconnect trigger) and costs a reconnect. Open.
- Restarting connman to enable debug: SIGUSR2 is not a debug toggle in this connmand, it
  killed it (systemd restarted it). Debug per file instead: `CONNMAN_ARGS=-d <files>` in
  /var/lib/environment/connman/*.conf.

## MPM, connman hold-off, charge limit (2026-10-31)

- Kernel i (patches 0038/0039): MPM up (`qcom_mpm` on GIC 229), the PSCI domain tree in OSI mode
  after one deferral. Power key, slide, double tap and SMS still wake. vlow/vmin after a real
  sleep: not yet read.
- connman 1.40+git26.6-100.qx1050 (`patches/connman/0001`, autoscan hold-off as a plain timer)
  installed with the kernel i boot. Whether the minute wake is gone: needs a sampled night.
- Charge limit overshoot: the PMI632 charges on during suspend, mce only acts on capacity
  updates, and the battery driver does not poll in suspend. After the reboot mce switched
  charging off at 8 s, on again at 184 s (below its 85 % resume threshold), and the phone then
  slept on the charger to 89 %. Patch 0040 keeps the system awake while charging; with the limit
  at 90 % it held the wakeup source for 95 s, released it when mce wrote 0, and the phone slept on
  at 0 mA battery current. The estimate settled from 90 to 88 % right after the stop at 4.196 V
  (reads low in that range; gauge item).
- Wanze recorded nothing from 22:51 to 02:37: after each boot wanze.timer was "elapsed" with no
  trigger (systemd 238, OnBootSec). Restarted by hand; drop-in
  `wanze.timer.d/10-qx1050-onactive.conf` (OnActiveSec=2min), not yet checked across a reboot.


## The night of 10-31 on kernel i, and USB after suspend (2026-10-31)

- 88 -> 25 % overnight unplugged (the gauge jumps; by voltage 4.196 -> 3.754 V, about 1.5 Ah in
  7.6 h, ~200 mA average; kernel h's rough figure the day before was ~100 mA). 287 suspend
  attempts, only ~25 min awake between them, so not the minute wake. But the printk clock ran
  9130 s inside suspend spans: a third of the "sleep" never reached the frozen s2idle state.
  36 RCU stalls during s2idle (one CPU busy, the others in cpuidle_enter_s2idle); kernel h: 1 in
  53 suspends. vlow/vmin still 0 with the MPM.
- USB data dies after suspend: after 5-19 suspends the gadget stays "not attached" (DCTL RunStop
  set, HS link "disconnected", no event on the host), a reboot restores it. Suspect: the USB30
  GDSC is PWRSTS_OFF_ON on sm6115, the glue writes the QSCRATCH VBUS override only at probe, so it
  is lost when the domain powers off in suspend. sc7180/sm6350/sm6375/sm8250 use PWRSTS_RET_ON
  for this domain. Kernel k (w-73 04e273465) does the same; not yet run. Check first:
  `qscratch.py` reads HS_PHY_CTRL (0x4ef8810) bit 20/28 after a suspend, `qscratch.py set`
  rewrites them.
- Wanze: systemd 238 drops a monotonic timer's first elapse if the system suspends before it is
  due (reproduced twice with the first suspend at 66 s); OnActiveSec does not help (tested).
  Handed to a-d-b; its proposal is to start wanze.service once at boot instead of OnBootSec.
- 12:15, end of a 45 min unplugged window on kernel i (pm_debug_messages on): the phone did not
  come back on VBUS and answers neither Wi-Fi nor USB. Not seen before on this kernel.

## Kernel m, and the cluster power-collapse parameter (2026-10-31 afternoon)

- The no-network boot 13:44-14:06 (kernel i): systemd-udevd hung (watchdog kill after 3 min),
  usb_moded "no network gateway"; that is why devmode never came up on that boot.
- Kernel m (0041-0043, 0044 module) on the phone from 14:09. USB: after a 10 min unplugged sleep
  neither link answered until an a-d-b port cycle (a fresh enumeration, so no proof). After the
  45 min window below it came up over USB without a cycle, but only 5.5 min after VBUS on.
  No `-22` brightness errors.
- 45 min unplugged (14:27-15:12, read at 15:17): 91 -> 84 % (~9 %/h, as kernel i's night),
  1514 s of 3037 s frozen (50 %), 24 suspends, 3 failed, 6 RCU stalls. Dropping the MPM changed
  nothing. The sleep-OCV anchor (0044) logged no line.
- Cause: the PSCI firmware (OSI mode) refused the cluster idle states. Kernel m counters per
  boot: cluster S1 rejected ~44 million times (usage underflowed to 2^64-1), the CPU
  power-collapse state ~1 million rejections per CPU, s2idle entries 3.8 M (cpu0) and 14.6 M
  (cpu4). The s2idle loop spun instead of sleeping.
  sm6115.dtsi's cluster power collapse is `0x41000044` (CPU field 4, rail collapse), but its only
  CPU state is `0x40000003`; cpuidle-psci sends the domain state in place of the CPU state, not
  ORed. sc7180/sm6375 pair `0x41000044` with a CPU state `0x40000004`; agatti (QCM2290) pairs
  `0x40000003` with `0x41000043`.
- Test (kernel m Image, dtb patched to `0x41000043` for cluster-sleep-0-1 and -1-1): in the
  first minute cluster S1 usage 374/148 with 19/4 rejections. GDHS (`0x40000022`/`0x40000042`)
  is still rejected by the ten thousand; not changed yet.
- Downstream `bengal-pm.dtsi` (android_kernel_fxtec_sm6115, lpm-levels) gives the encoding: CPU
  mode in bits 0-3 (wfi 1, pc 3; no rail collapse), cluster mode in bits 4-7 (shift 4: l2-wfi 1,
  l2-gdhs 2, l2-pc 4), system mode in bits 8-11 (shift 8: system-pc 3), plus the PSCI affinity
  level in bits 24-25 and power-down in bit 30. Cluster PC over CPU PC is therefore `0x41000043`
  (agatti's value), cluster GDHS `0x41000023` on both clusters; mainline's GDHS values
  `0x40000022`/`0x40000042` lack the affinity bit, and the cluster1 one carries the PC field.
- Kernel h (no MPM, no 0038) probably ran OSI too (kernel m sets the topology up at 0.07 s with
  no deferral), but no h boot log with the line survives; its ~100 mA night stays unexplained.
- Result, PC `0x41000043` only (15:28-16:02, 34 min unplugged): 1940 s of 2025 s frozen (96 %, kernel
  m 50 %), 0 RCU stalls (kernel m 6), s2idle entries 39/35 instead of millions, cluster S1 entered
  in s2idle. Voltage 4.261 -> 4.216 V, both at about 0 mA; the 97 -> 90 % gauge figure starts from
  the boot jump and says nothing. Then with GDHS `0x41000023` as well: 2 min after boot cluster0
  GDHS used 3509x with 318 rejections (before: 0 and 57 k), cpu0 PC rejections 518 (before
  12.8 k). Patch 0045 (w-73 4b7f598e0) puts both values into sm6115.dtsi. The phone runs the
  dtb patched in place (scratchpad fdtpatch.py), kernel m's Image.
- Still open: a real drain figure (needs an unplugged window long enough for the voltage, or a
  night), XO shutdown (vlow/vmin still 0: no MPM, no system level), USB answering 4-5 min after
  VBUS on rather than at once.
- 60 min unplugged on the fixed dtb (16:05-17:06): 3499 s of 3622 s frozen (96.6 %), awake ~2 min
  in the hour, 0 RCU stalls, 9 failed suspend attempts; gauge 90 -> 86 % (~4 %/h against ~9 %/h on
  kernel m; the end reading was under charge). XO shutdown still 0: that is the floor now.

## USB after plug-in (kernel n, 2026-10-31)

- The 4-5 min until USB answered after VBUS on: nothing woke the phone on plug-in. The charger
  driver polls, and the PMIC Type-C port's interrupts were not wake sources. Patch 0046 makes
  VBUS, CC state and attach/detach wake sources (with `wakeup-source`, 0047). Three plug-ins
  after 4 min asleep: resumed within 2 s each time.
- Once (round 1) USB still came up only after 375 s: the A16 hub port first read as a charger
  (APSD 0x08 DCP, then 0x28 DCP+QC2.0), so usb_moded stayed in charger mode, until APSD read 0x04
  (CDP) at 17:17:27. Other plugs read 0x04 or 0x02 (OCP). Asked a-d-b whether the port does this
  at power-on. `typec port0-partner: PM: parent port0 should not be sleeping` with an aborted
  suspend at each plug-in during suspend entry: the wake doing its job, noise.
