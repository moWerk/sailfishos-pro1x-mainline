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
- Plugged into the PC/hub with charging stopped at the limit, the phone slept and USB went dark
  (a host cannot wake it through the gadget). Patch 0048 keeps it awake on any non-DCP port, as
  downstream dwc3-msm does. Reloading the module re-enabled charging behind mce's back (probe);
  after a reload write 0 to charging_enabled if mce has charging off.
- 17:28:39: a-d-b's stray-SSH aligner power-cycled the port unasked (it probes the AsteroidOS
  default 192.168.2.15, the phone is on .19); no measurement was running. a-d-b is fixing it; take
  its hold on the port for test windows.

## Sleep OCV and XO shutdown (2026-10-31 evening)

- 0044 never anchored: QG_STATUS2 0x03 (GOOD_OCV set), S3 OCV 0x5471 = 4.2075 V, unchanged.
  The QG takes an S3 OCV only after the battery current stays under its S3 entry threshold for
  a few FIFO samples (binding qcom,s3-entry-ibat-ua, 0-155 mA; we do not program it, so the
  hardware default applies). With XO never shut down, sleep current stays above it; the
  latched value is from resting on the charger at 0 mA. 0044 waits on XO shutdown.
- rpm_master_stats did not autoload (no match on boot; `modprobe rpm_master_stats` works).
  Kernel n, 34 min up, mostly on USB: MPSS 6353 XO shutdowns, CDSP 244, APSS 0 (never a
  shutdown reported to the RPM), ADSP 0 with active-core bitmask 0x1, TZ 0.
- Mainline sm6115 has no system-level domain idle state. Downstream bengal-pm.dtsi has a
  "system" level above the clusters (system-wfi 0, system-pc 3, psci-mode-shift 8). Without it
  the APSS never asks the RPM for sleep, so XO shutdown cannot happen whatever the other masters
  do. A system level means the GIC is off in sleep and wakeups go through the MPM, which needs
  the vMPM wakeup timer that mainline does not write (see the MPM entry above). So the order
  is: MPM as the system-level domain, plus the timer, plus a system PSCI state (by the encoding
  0x42000343: affinity level 2, system 3, cluster 4, CPU 3; to be checked against the firmware).
  ADSP's bitmask 0x1 is a second, separate blocker.
- Perspective on 0045: by gauge the fixed dtb does ~4 %/h unplugged, kernel h did ~3 %/h (2 % in
  39 min). 0045 removes the regression that OSI mode with refused cluster states caused; it does
  not lower the floor, which is XO never shutting down.
- ADSP never power-collapses (kernel n, whole boot: shutdown count 0, active-core bitmask
  0x1). Not the sensor client: with sensorfwd and qx1050-hexagonrpcd stopped and no fastrpc
  holder, 5 min, still 0. Not a stream: every PCM closed, both pulse sinks SUSPENDED.
  Candidate: the LPASS votes through q6afe. With everything runtime-suspended (rx/tx/va macros,
  both SoundWire masters and their wcd937x slaves), clk_summary still shows LPASS_HW_DCODEC 4,
  RX/TX_CORE_NPL_MCLK 1-4, RX/TX_CORE_MCLK 1-2 and fsgen 2 enabled at 19.2 MHz.
  lpass-macro-common takes the macro/dcodec votes at probe and drops them only at removal; the
  macros' own pm_clk set is released in runtime suspend, but the output clocks the macros
  register for SoundWire keep users counted. The macros cannot be unbound
  (suppress_bind_attrs); unbinding only the card left the clocks on. Not yet shown that these
  votes are what keeps the ADSP awake; that needs a patched kernel (drop the votes in the
  macros' runtime suspend) and an audio regression check (speaker, earpiece, calls, the
  first-probe silence), so it waits for the author's go.

## Kernel p: system-level sleep under the MPM (2026-10-31, experimental)

- Kernel n + a system power domain above both clusters (`system-sleep-0`, `0x42000343`:
  system 3, cluster PC 4, CPU PC 3, affinity level 2; latencies from downstream system-pc) whose
  parent is the MPM (0039's node, the clusters no longer its direct children). The MPM genpd
  uses the CPU governor, and its power-off writes the earliest hrtimer of the CPUs below it as
  an absolute arch timer count to the vMPM TIMER0/1 before the IPC to the RPM. During system
  suspend (s2idle, pm_suspend_target_state set) it writes "never": kernel timers are frozen
  then and the wakeup interrupts decide.
- Wake paths with the GIC off: only the MPM pin map (TSENS, LPASS SoundWire, DWC3 ss_phy, LPASS
  LPI, SPMI = power key, RTC, typec/VBUS, QUSB2 DP/DM; tlmm through wakeup-parent: touch,
  keyboard slide). Not mapped: WCN3950 Wi-Fi, the modem's IPC lines. Expect Wi-Fi not to wake
  the phone; calls/SMS unproven.
- Recovery if it does not wake: /api/on on 1-3.3.4/4 (VBUS -> typec IRQ -> SPMI MPM pin), else
  the power key. Backup of kernel n: /root/boot_a.pre-k-p.img.
- Correction (kernel q with a temporary clk-core debug log, 2026-10-31): the LPASS clocks are
  NOT held. Every prepare is matched by an unprepare; between bursts LPASS_HW_DCODEC's
  clk_prepare_count is 0. The counts of 1-4 seen before came from reading clk_summary itself:
  its "hardware enable" column calls clk_pm_runtime_get_all, which runtime-resumes the macros
  (providers of fsgen and the mclk outputs), and their pm_clk resume prepares the q6afe clocks.
  Read `/sys/kernel/debug/clk/<name>/clk_prepare_count` instead. So the audio votes do not keep
  the ADSP awake; no audio patch.
- Kernel p result (10 min unplugged): APSS shutdowns 326 -> 8965 (the RPM sees the CPUs sleep
  for the first time), MPM genpd off, no stalls, timers on time. But USB answered only 868 s
  after VBUS on, and the system state was rejected 13431 times: the SPMI arbiter (power key,
  RTC, typec) still went to the GIC, which is off in system sleep. Kernel q routes it through
  MPM pin 86 (as agatti): USB 4 s after VBUS on, system state 690 uses / 50 rejections, 81 %
  frozen over 5 min, 0 stalls.
- XO shutdown still 0 on q. The ADSP never sleeps (shutdown count 0, bitmask 0x1), and it is not
  the audio clocks (see the correction above) nor the sensor daemons. Stopping remoteproc1 to
  test it hung in the APR teardown (q6afe "packet not transmitted", remoteproc stayed
  "running"); a forced reboot recovered. Next candidates: what the ADSP firmware itself keeps
  running (SSC always-on sensors, the registry), and the RPM votes the HLOS holds for LPASS.
- Modem wake on q (tested at the bench, 2026-10-31 21:30): an SMS arrived about 1 min late and
  a call rang 20-25 s after the caller heard it ringing; the wake IRQ log showed only the
  pm8xxx RTC alarm (IRQ 131), so the next timer woke the phone and not the modem. The modem's
  glink (SPI 68) and SMP2P interrupts have no MPM pin. Double tap, the hall sensor and the
  power key woke at once (TLMM and SPMI go through the MPM). Downstream's mechanism for this is
  not found yet. So 0050/0051 stay out of the daily kernel.
- Daily kernel s (2026-10-31 23:10): kernel q's Image with kernel n's dtb, so no system domain
  (0049 is in the Image but idle without the MPM node). Calls and SMS should wake at once
  again (bench check 2026-10-31 23:45: SMS and call both instant), at the cost of no APSS sleep report to the RPM.
- The ADSP RPM master entry also shows no bringup request ever (`Last bringup req @ 0`): the
  ADSP never asks the RPM to sleep, it is not refused. Its own sleep stats (SMEM) are not
  exposed by mainline qcom_stats on sm6115; that is the next diagnostic.
- Careful: reading whole PMIC regmaps through debugfs (`/sys/kernel/debug/regmap/*/registers`)
  crashed the phone into the Qualcomm dump mode (05c6:900e) within a minute; only a long power
  key press got it out. Read named registers only.
- The DCP-first detection is phone-side: 16 plug-ins with the phone awake read SDP/OCP/0x00
  and never DCP; both DCP cases came with a plug-in to a sleeping phone. Patch 0052 re-runs APSD
  once: DCP, QC2, re-run, SDP, USB in 10 s.
- Wi-Fi in sleep: 0/73 pings answered over the 40 min window on q. Not a regression: WoWLAN only
  wakes on magic packet and disconnect, pings never woke the phone (kernel m timed out too).
- Wanze 4d2f6cd + 56a8b01 on the phone (realtime OnCalendar timer, one row per wake).

## Sleep current measured directly (2026-11-01 night)

- The PMI632 QG keeps sampling while the SoC sleeps (S2: 256 samples x 300 ms per FIFO entry,
  8 entries). A debug module (qgsleep: clears the FIFO with the downstream master hold/clear at
  PM_SUSPEND_PREPARE, reads FIFO and the real-time accumulator at PM_POST_SUSPEND) prints the
  mean battery current of every sleep. Awake on USB the FIFO stays empty.
- Kernel s, screen off, unplugged, 4 min windows: baseline 79 mA; sensorfwd + hexagonrpcd
  stopped 79.7 mA. Three windows meant as Wi-Fi/BT/airplane changed nothing (connmanctl is not
  installed) and read 80.9 / 82.8 / 81.5 mA: the spread is about +-2 mA. ~80 mA matches the
  4 %/h seen before, and is 5-10x what this class of SoC should draw with XO on.
- TLMM 65 is high with the screen off. The Fxtec downstream DT (bengal-idp.dtsi, panel node
  `dsi_sw43404_boe_fhd_video`) gives it as the panel's `qcom,platform-en-gpio`: dsi_panel.c
  drives it high after all panel rails and before reset, low first at power off. In our DT it
  is only a pinctrl state and the panel's four supplies are dummy fixed regulators, so nothing
  ever switches it; the bootloader leaves it high.
- 2026-11-01 morning, same conditions, GPIO 65 held low from userspace (GPIO v2 uAPI):
  48.5 mA; baseline right after with it high again 72.9 mA. Two more windows still had it low
  (the holder's sleep stops in suspend): 55.0 mA with BT off, ~55 mA in airplane mode, so BT
  and the radios show no saving. Wi-Fi off (from the night, recovered from dmesg): 77.6 mA.
  GPIO 65 is worth 20-25 mA, about a third of the sleep drain.
- Fix (kernel t, dtb only): GPIO 65 becomes the enable of `disp_vci_supply` (enable-active-high,
  regulator-boot-on, the panel_en_n pinctrl moved from the panel to the regulator). The
  bf060y8m-aj0 driver enables VCI last and disables it first, the downstream order.
  On the phone 09:48: display and power key fine; with the screen off VCI is disabled and
  GPIO 65 low; two windows 49.5 / 49.3 mA. Source form: patch 0053. Kernel t is the daily.
- The other outputs high in sleep on kernel t: TLMM 26 (pm8008), 68 (haptics), 71 (touch),
  92 (WCD937x), 111 (keyboard expander) are reset lines held deasserted by their drivers, as
  intended. TLMM 34, PM6125 GPIO 1 and PMI632 GPIO 6 (the reference boards' LCD backlight
  enable) are unclaimed, high from the bootloader; all three held low: 47.7 mA against 49.4,
  inside the +-2 mA spread. Nothing more in GPIOs; the rest is XO staying on (the ADSP).

## The ADSP never sleeps, whatever runs on the HLOS side (2026-11-01)

RPM master stats, ADSP: shutdown count 0, last bringup request 0, read 3-4 min after boot
(CDSP 97 shutdowns in the same time). Kernel t, each case a fresh boot:

- normal boot; sensorfwd and hexagonrpcd stopped 60 s, PulseAudio frozen 60 s: 0
- sensorfwd masked and hexagonrpcd disabled at boot, nothing holds /dev/fastrpc-adsp: 0
- also `apr` blocked (no q6 audio/voice services; macros, LPI pinctrl, SoundWire unbound): 0

HLOS side clean in all of them: every LPASS clock (q6afe-clocks incl. the vote clocks) at
prepare/enable 0, lpi_cx/lpi_mx genpds off, all three q6v5 handover interrupts fired once (proxy
votes released). The master stats node matches downstream bengal-pm.dtsi (0x45f0150, 0x1000
per master, APSS MPSS ADSP CDSP TZ). So the ADSP stays awake by its own choice from boot on,
with no client. Next: a downstream reference reading (hybris Pro1-X, same firmware): does this
ADSP firmware ever power-collapse there, and what does the HLOS do at ADSP boot that we don't.

Downstream reference (hybris Pro1-X, 4.19.157 halium, same ADSP build ADSP.VT.5.4.1-00376-KAMORTA-2):
awake on USB the ADSP also stays up (11 shutdowns, all in early boot, then none to 280 s; vlow
and vmin 0). Unplugged with the screen off for ~30 min: ADSP 456 shutdowns / 455 XO
shutdowns, RPM vmin 1129, last vmin sleep 320 s, 42 suspends. So the firmware can sleep; on
mainline it does not, also in suspend.

Mainline suspended, each a 5 min QG window on kernel t/u:
- sensor stack never started at boot: ADSP 0, vmin/vlow 0, 48 mA
- kernel u (`qcom,vmids = <22 37>` on the ADSP fastrpc node, as downstream
  `qcom,adsp-remoteheap-vmid`) + the audio static PD created (INIT_CREATE_STATIC "audiopd",
  3 MB as qualcomm/fastrpc adsprpcd audiopd) + hexagonrpcd attached to the root PD: ADSP 0,
  46.7 mA. The audio PD comes up cleanly; the root PD at once asks for `lowi/lowi_lp.conf`,
  which hybris lacks too.

Same on both sides: glink channels to the ADSP, SMP2P sleepstate (bit 12, out + `_see` in),
master stats layout, the in-kernel pd-mapper has the sm6115 domains, no clk_ignore_unused,
RPM QDSS/IPA/snoc_lpass and all LPASS clocks at 0, ADSP glink traffic 12 interrupts/min (as
the CDSP, which sleeps). Downstream extra, not yet tried: smp2p `rdbg` entries; downstream
audio's q6core/adsp-loader handshakes.
- The SMEM subsystem sleep records (605 modem, 606 ADSP, 607 CDSP) exist but read 0 for all
  three, the modem included while the RPM counts its shutdowns: not maintained on this firmware.
- The ADSP is quiet (6 glink interrupts in 30 s) yet never sends a bringup request to the RPM.
- USB did not come back after the 7th window (01:07), no 900e; series stopped. The phone was
  fine: no USB event all night, charger saw the port as DCP (APSD 0x04), usb_moded stayed in
  charger mode (the 0046 row's DCP misread). Same after the airplane window in the morning; one
  more VBUS cycle brought it back in 5 s.

## The ADSP's LPASS core vote leaks: q6afe drops the vote handle (2026-11-01 afternoon)

- Mainline logs `qcom-q6afe ...: cmd = 0x100f6 returned error = 0x1` / `Unknown cmd 0x100f6`
  (seen at 24 s and 121 s on kernel t; earlier filed as harmless). 0x100f6 is
  `AFE_CMD_REMOTE_LPASS_CORE_HW_DEVOTE_REQUEST`: the ADSP refuses every devote.
- Cause: the DSP answers the vote with the client handle it assigned (`payload[0]`, the token
  echoes the hw block; downstream stores it in `lpass_hw_core_client_hdl[token]`). Mainline's
  callback reads that payload as a basic result and never fills `*client_handle`, so each devote
  names a handle the DSP never handed out and the vote stays. The HLOS clock counts are balanced
  (as found on kernel q); the leak is on the DSP side, invisible from clk debugfs.
- Patch 0054 keeps the handle per hw block and returns it; the devote result is a known command.
  q6afe is a module, so the test is kernel t + the new `q6afe.ko` (kernel x build, only q6afe.o
  changed).
- Result: no 0x100f6 errors; the ADSP power-collapses on mainline for the first time (91 XO
  shutdowns 2 min after boot, `Last bringup req` non-zero, 108 after a 5 min window).
- Why the earlier `apr` block test still read 0 is open: with no APR there is no vote at all.
  Possibly the LPASS core starts voted at ADSP boot and only a devote releases it; not shown.
- Sleep current did not drop: kernel t + 0054 one 148 s sleep at 55.8 mA; kernel w (below) +
  0054 53 mA, against 49.4 (kernel t) and 45.3 (kernel w) without it. Fresh boots, single
  windows; an A/B on the same kernel is next.
- Kernel w, a re-run of 0050/0051 as a dtb edit (done again by mistake, the notes above already
  had it): APSS 384 shutdowns while awake, 967 after a window, system state used in s2idle.
  With 0054 too, every master power-collapses in one window (APSS 1234, ADSP 98, MPSS 1018,
  CDSP 64), yet APSS XO shutdown 0 and vlow/vmin 0: the APSS still holds an XO vote. Awake,
  `bi_tcxo` (RPM_SMD_XO_CLK_SRC, the sleep-set XO) has 31 prepares, behind it gpll0 (UFS, USB,
  QUP s3), the USB mock_utmi/phy_aux RCGs and dispcc mdss_ahb. Which of these stay prepared in
  suspend is the next probe (clksleep, a noirq-suspend snapshot of 226 rpmcc/gcc/dispcc/gpucc
  clocks; its first version also sampled from the CPU PM idle path and hung the phone).
- Downstream reference, AP held awake by a wakelock and unplugged: ADSP 0x2b4 -> 0x2ee shutdowns
  in 2.5 min, APSS XO shutdowns 0xa46, RPM vmin +450/min. Downstream reaches vmin from cpuidle
  alone, not only in suspend. Downstream's root `interrupt-parent` is its MPM GIC wrapper, but
  its pin map (qcom-mpm-bengal.c) has no modem lines either, so its modem wake from system-pc
  does not come from MPM pins; the mechanism is still not found.
- Also closed: the ADSP's `cpuv_restriction_cold` QMI cooling device (TMD service 0x18,
  instance 1) reads level 0 on mainline without any client, as do the modem's and CDSP's TMDs;
  not a vdd floor. LPASS clock prepare counts sampled 120x over 60 s: always 0.

## vmin on mainline: UFS clocks, UFS PHY retention, dispcc XO (2026-11-02)

- clksleep, fixed: `of_clk_get_from_provider()` returns NULL, not an error, for unpopulated
  indices; the earlier builds kept those and oopsed in noirq suspend (three hangs, no log). Now
  `IS_ERR_OR_NULL` and the snapshot is read from debugfs after the wake.
- In suspend `bi_tcxo` was held by two chains: the UFS clocks through gpll0, and
  `disp_cc_mdss_ahb_clk` -> `disp_cc_mdss_ahb_clk_src` -> `bi_tcxo`.
- UFS: prepare and enable counts climbed by one per runtime resume (125 -> 132 in 40 s, >130
  after 15 min), so the clocks never stopped, even with the host runtime-suspended. Upstream
  fixed it in the 7.4 merge window (f07317a8d57f): the lane clock helpers enabled every host
  clock. Backported as 0055. A guard of my own in `ufs_qcom_enable_lane_clks()` first stopped the
  leak but kept bulk-disabling the core clocks; dropped.
- With the UFS clocks really gating, every system-domain suspend risked a crash: UFS PA error in
  suspend, hibern8 exit (UIC 0x18) timeout, SError in `ufshcd_err_handler`, panic, watchdog,
  crash dump (05c6:900e). The log came from a RAM dump (an own pyusb Sahara memory-debug client:
  HELLO, HELLO_RESP mode 2, MEMORY_READ_64 of the debug table and DDR; the printk ring buffer
  greps fine). Same signature as upstream's Milos fix (PHY state lost in system suspend), so
  0056 sets `no_phy_retention` (spm_lvl 5). Sahara RESET (cmd 7) reboots out of crash dump mode
  without the power key; `kernel.panic=5` makes a panic reboot instead of waiting for the
  watchdog.
- dispcc: the DSI PHY's pm_clk keeps its iface clock prepared for good (pm_clk prepares at add
  time). 0057 feeds dispcc from RPM_SMD_XO_A_CLK_SRC.
- Result with 0050 (system domain), 0054, 0055, 0056, 0057: nothing on `bi_tcxo` in suspend,
  RPM vmin for 85 % of 5 min windows (37.5, 40.4 mA) and 84 % of a 15 min window (45.0 mA,
  wakeups: IPA 7, RTC 4, WLAN 1, VBUS 1). A/B without 0057: `bi_tcxo` held, vmin 0, 74.9 mA.
  On the daily dtb (no system domain) the APSS never makes an RPM-level sleep, so the XO fixes
  cannot show there (kernel z: 56.4, 59.9 mA).
- Still open for the daily kernel: modem wake from system sleep (0050's late SMS/calls). The
  IPA interrupt (SPI 289, not MPM-mapped) did wake the system from s2idle with the system
  domain, so unmapped modem-side interrupts may wake after all; needs a timed SMS test.
- Display on/off with 0057 not yet checked.

## The floor with vmin: USB runtime PM, iphb wakes, the modem (2026-11-02, night)

Kernel zz-yw2 (0050, 0054-0057), screen off, unplugged, QG windows.
- 2 h night window: 39.8 mA, vmin 86 % of the time, 40 suspends, no crash. Wakes: RTC 35, WLAN 11
  (every 10 min, the AP's group rekey), IPA 7, VBUS 2. The RTC alarms are all dsme iphb: in a
  40 min window with dsme at debug level (`dsmetool -l 7`) connmand 4 (~6 min keepalive) and
  msyncd 3. Worth about 1 mA; the floor is the rest (an 18 min sleep alone read 33.6 mA).
- No saving: goodix wakeup off (44.0 mA; 0035's scanning in suspend costs nothing measurable),
  BT rfkill-blocked (~41 mA in the clean sleeps; the WCN rails stay on for Wi-Fi).
- The DWC3 core forbids runtime PM at probe (`power/control` = on); Android's init allows it,
  SFOS did nothing. Unplugged, the controller stayed active with its PHY supplies L4, L12 and
  L15 on. With `auto` it runtime-suspends and the three supplies go off: clean sleeps 29-32 mA
  instead of 40-42 (two windows), a 30 min window 34.0 mA mean with vmin 95 %. USB gadget mode
  comes back on replug. droid-config: `70-qx1050-usb-runtime-pm.rules` (at bind, since probe
  resets the attribute).
- Correction (after a reboot, 06:21-09:00): the rule sets `auto` at boot, but in that boot dwc3
  never runtime-suspended unplugged (90 min window 39.6 mA, clean sleeps 34-39). Unplugged:
  role device, UDC "not attached", link Suspend; usb_moded mode "undefined" with the rndis
  gadget still bound. Neither an on/auto toggle after the unplug nor unbinding the UDC let it
  suspend, so a runtime-PM reference is held that the earlier boot did not have. The rule is
  necessary, not sufficient; finding the holder needs CONFIG_PM_ADVANCED_DEBUG (runtime_usage,
  runtime_active_kids).
- Remaining spikes: some sleeps read 110-170 mA for minutes. A per-wake log (/root/snaploop.sh:
  vmin, per-master XO and shutdown counts) shows MPSS neither sleeping nor counting a shutdown
  through them, and each ends with the RTC alarm: the modem stays up, probably on traffic it
  cannot wake the APSS for (the same gap as the late SMS). Modem glink is IRQ 209 (SPI 68),
  smp2p-mpss IRQ 18 (SPI 70), both without wake arming; glink-smem requests IRQF_NO_SUSPEND.
- Why dwc3 stays active (2026-11-02 morning, kernel zd = zz + CONFIG_PM_ADVANCED_DEBUG): unplugged
  it shows runtime_usage 0 and no active children, yet stays active, also with the UDC unbound.
  The suspend is never tried: the 5000 ms autosuspend delay runs on timers that stop in s2idle,
  the wakes last a few seconds, and dwc3's system resume resets the runtime state, which drops
  the pending timer. It suspends only when the phone happens to stay awake 5 s after the unplug
  (the boots where it worked). With autosuspend_delay_ms 0 it was suspended in every unplugged
  snapshot, but a replug then never enumerated (it re-suspends before the host resets it).
- Also seen: the legacy glue (dwc3-qcom-legacy) sets VBUS valid at probe for good (no extcon in
  the DT), so an unplug can look like a bus suspend (link Suspend). A flattened node
  (`qcom,sm6115-dwc3`, `qcom,snps-dwc3`, as agatti upstream; the binding has sm6115) works with
  dwc3-qcom, whose role hook clears VBUS valid; on its own it did not change the suspend, and
  with autosuspend 1000 ms the phone hung in a suspend (no oops in the RAM dump, watchdog, 900e).
  Back on zz-yw2 with the legacy node.
- With dwc3 suspended the clean sleeps still read 34-38 mA in that boot, so the 29-32 mA of the
  night boot is not explained by USB alone. Open.

## The modem wakes the system: daily kernel with vmin (2026-11-02, afternoon)

- Mechanism of the late SMS with 0050: the glink-smem edge interrupt is IRQF_NO_SUSPEND, so it
  is handled in s2idle, but reports no wakeup. Seen in the kernel log: at the SMS the s2idle loop
  woke (a 124.9 s sleep followed by a 12.4 s one in the same suspend), went back to sleep, and
  lipstick showed the SMS 0.3 s after the next real wakeup (VBUS). Downstream bengal reports a
  wakeup from qrtr for every packet from the modem (QRTR_WAKEUP_MS 0).
- 0058-0060: the edge device reports a hard wakeup event from the interrupt when wakeup is enabled,
  enabled at boot by `wakeup-source` on the mpss edge node (adsp/cdsp edges stay disabled).
  SMS while suspended on battery: 6 and ~20 s after sending. A call rang before the caller heard
  the ringback (author).
- Cost: the modem edge woke the system 2 times in 60 min; 40.0 mA over 60 min (clean sleeps
  35-38 mA, a few 48-55), 38.6 mA over 30 min. The 110-170 mA stretches of the night (modem up,
  waiting for the host until the next RTC alarm) did not appear in these 90 min.
- The tested dtb had come from in-place dtb edits; the dtb built from the series (MPM node under
  the RPM, `system-sleep-0`) was booted and measured the same (38.6 mA, vmin ~81 % since boot,
  no UFS errors). **Daily kernel since 2026-11-02 (kernel zs): Image of kernel zz, dtb and
  qcom_glink_smem.ko from the series up to 0060.** It has RPM vmin with prompt SMS and calls.
- sync_state check (a lead from the Pixel Watch port, where GKI's clk hold-state pins every
  boot-on clock until all consumers of a provider have bound): only gpucc is unsynced here
  (596a000.gmu has no driver of its own), both GPU GDSCs are off, mainline has no clk hold-state,
  no interconnect or rpmpd provider is unsynced, nothing deferred. Nothing to gain.
- **Correction (18:10): the first 0058 broke modem recovery.** At 16:08 the WLAN firmware (on the
  modem) asserted (`wlan_process ... wal_css_handle_sleep_failure`, right after a reassociation at
  a WoW resume); the modem crash recovery then oopsed in `device_del` under
  `qcom_glink_smem_unregister`: the edge's wakeup class device is a child of the edge, and
  `qcom_glink_native_remove()` unregisters every child, so it was freed and then unregistered
  again. Modem and Wi-Fi stayed down until a reboot (about 1 h 45 min; the drain windows only
  read current and did not notice). Reproduced with the debugfs crash trigger
  (`echo 2 > /sys/kernel/debug/remoteproc/remoteproc0/crash`). Fixed 0058: a standalone wakeup
  source (`wakeup_source_register(NULL, ...)`), as downstream qrtr. With it the recovery
  completes ("remote processor modem is now up"), but the phone then went into crash dump mode
  (900e) with nothing more in the log (RAM dump `ramdump-zf`); whether that happens on modem
  recovery without 0058 too (the October note "stopping remoteproc0 crashes the kernel"
  suggests so) is open. Also open: whether the WLAN firmware assert recurs.
- Same test on the original qcom_glink_smem.ko (without 0058), 18:13: the recovery runs the same way
  to "remote processor modem is now up", then the phone goes into crash dump mode with nothing more
  in the log (RAM dump `ramdump-orig`). So the silent crash after a modem recovery predates 0058;
  0058 (fixed) neither causes nor cures it. Any modem crash (the WLAN firmware one included) takes
  the phone down to 900e until a long power press. Open, its own item.
