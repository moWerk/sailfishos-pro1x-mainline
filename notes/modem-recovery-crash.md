# The phone crashes right after a modem recovery

Status 2026-11-02 20:30: **workaround in place**, root cause unknown. A modem crash
(subsystem restart) put the phone into 900e crash-dump mode about a second after the
recovered modem was up, whenever ath10k had only run its boot-time power sequence. One
Wi-Fi power cycle after boot prevents it: `qx1050-wifi-cycle.service`.

## What happens

- Kernel 7.3 (zs), modem crash triggered with
  `echo 2 > /sys/kernel/debug/remoteproc/remoteproc0/crash` (a fatal error, as a real modem
  crash). The real case on 2026-11-02 16:08 was a WLAN firmware assert
  (`wal_css_handle_sleep_failure`, the WLAN firmware runs on the modem) after a WoW resume:
  same restart path, different trigger.
- The recovery runs normally: ath10k `firmware crashed!`, IPA crashed/offline/starting/
  running, `remote processor modem is now up`. Then the phone resets into 900e. Nothing more
  is logged: the kernel log in the RAM dump (Sahara, DDRCS1_1) ends at "modem is now up", no
  oops, no panic, no watchdog text. So a hang or a reset below the kernel (bus or XPU fault,
  watchdog bite), not a kernel panic.
- It is not 0058: the original `qcom_glink_smem.ko` behaves the same (RAM dump
  `ramdump-orig`).

## Isolated (2026-11-02, pro1x#2, debugfs crash each time)

| State of ath10k when the modem crashes | Result |
|---|---|
| loaded at boot, never cycled (stock modules twice, traced modules once) | 900e, 3 of 3 |
| unbound (`ath10k_snoc/unbind`) | recovers |
| reloaded after boot (`rmmod`/`modprobe`), first modem crash of that boot | recovers |
| Wi-Fi switched off and on in connman, no reload (first and second crash of the boot) | recovers, 3 of 3 |
| second and third crash of a boot whose first crash ran without Wi-Fi | recovers, 2 of 2 |
| `qx1050-wifi-cycle.service` at boot, then two crashes | recovers, 2 of 2 |
| the same boot after 30 min on battery (6 suspends, 20 min asleep, 32.1 mA), third crash | recovers |

- ath10k is passive in the fatal second. ath10k_core/ath10k_snoc built with a print before
  every step of the QMI events, power up/down, hif start/stop, recovery and
  `ath10k_core_start` (no kprobes in this kernel, `kconfig debug 0`): after the crash the RAM
  dump shows `server_exit` and the crash dump, then nothing. No `server_arrive`, no recovery
  work, before the reset.
- At boot ath10k already powers down and up once (firmware probe at 16 s, interface start at
  20 s), and that does not protect. The protecting cycle ran later, with the station
  associated. One visible difference: QMI `wlan_disable` took 65 ms at boot, 1.6 s in the
  connman toggle.
- The rails are not the difference: Bluetooth on the same WCN3950 holds L16, L17 and L23;
  a Wi-Fi power down only switches L8 (vdd-0.8-cx-mx), at boot as in the toggle.
- The Wi-Fi node already has `qcom,msa-fixed-perm` (no MSA hypervisor assignment).
- Fits the October boots (`modem-dog-crash.md`): the modem crashed then before ath10k had
  come up, and those recoveries worked.

## Workaround

`qx1050-wifi-cycle.service`: waits for wlan0 to be up, 10 s more for the connman restart of
`qx1050-wlan-mac.sh`, then sets the connman wifi technology `Powered` false and true (D-Bus,
with retries). Costs one reassociation about 30 s after boot. `/etc/sysconfig/qx1050-wifi`:
`CYCLE_DISABLE=1` skips it. If Wi-Fi is off at boot nothing is cycled; whether a modem crash
is then safe is not tested.

## Open

- Does the protection last through hours of suspend and WoW? (The 16:08 crash came hours
  after boot.) 30 min asleep, then a crash: recovered. Hours not yet tested.
- Root cause. Candidates: the state the WLAN firmware keeps after its first mission-mode
  session at boot (calibration, BDF download, `cal_report`) that a later disable/enable
  clears; or WCN3950 state that only a disable after association resets. Next tools: the
  modem's own crash reason via the SSR reason in SMEM, if the reset leaves any; downstream's
  icnss behaviour on SSR (it votes and power-cycles the WCN itself).
- Lead for the root cause (2026-11-02 21:15, untested): the WCN digital rail L8
  (vdd-0.8-cx-mx). Downstream icnss votes it at 640 mV (`qcom,vdd-cx-mx-config = <640000
  640000>`), and mainline's qrb4210-rb2.dts pins L8 at 640 mV. The Pro1-X DT allows 400-728 mV
  and ath10k sets no voltage, so the APSS vote is 400 mV (regulator_summary). If the modem's
  own vote for the rail drops while it restarts, the WCN may brown out. Test: L8 fixed at
  640000 in the board DT, `CYCLE_DISABLE=1`, then modem crashes on fresh boots. Also
  different: downstream reassigns MSA permissions around a fatal crash (msa-fixed-region),
  mainline uses `qcom,msa-fixed-perm`.
- The 2 h battery test was stopped at 21:15 (the author needs the SIM for travel).
- **The SIM is part of the condition** (21:00-21:22, SIM removed, Wi-Fi cycle disabled, first
  modem crash after a fresh boot each time): L8 fixed at 640 mV recovered 4 of 4, but the
  daily DT with L8 at 400 mV also recovered 3 of 3. All the 900e runs above had the SIM in.
  So the crash needs a SIM (a registered modem: LTE/IMS activity, WLAN-LTE coexistence?)
  together with the boot-time ath10k state, and the L8 runs say nothing yet.
- **The WLAN firmware assert recurs, and its trigger is a reconnect racing suspend**
  (2026-11-02/03 night, no SIM, daily zs with the Wi-Fi cycle, 7 h on battery): 5 asserts
  `wal_css_sm.c:wal_css_handle_sleep_failure:1639`, each a modem crash, all recovered. Every
  one has the same sequence: in WoW the AP deauthenticates the phone with Reason 7
  (CLASS3_FRAME_FROM_NONASSOC_STA: the AP had already dropped the station), the phone
  resumes and suspends again at once, mac80211 authenticates during the freeze
  (`authenticated`, no `associate with`), ath10k enables WoW, and the firmware asserts in
  its sleep state machine. 30 Reason 7 deauths and 36 reassociations in 7 h: the AP times
  the sleeping station out about every 14 min. ath10k disables the firmware's STA keepalive
  on purpose (`ath10k_mac_vif_disable_keepalive()`, mac.c), so in WoW nothing keeps the
  station alive at the AP. With the SIM in, any of these crashes may be the fatal kind.
- **Test modules (2026-11-03, ath10k_core only, out of tree, 3 h battery windows, no SIM).**
  A deauth "races" when the system enters suspend again before the next `associated`.
  | Module | mA | Deauths | Raced | Asserts |
  |---|---|---|---|---|
  | stock (night, 7 h) | 33.2 / 34.8 | 34 | 34 | 5 |
  | firmware NULL-frame keepalive 60 s in WoW | 36.2 | 15 | not counted | 1 |
  | + EAPOL/GTK/4-way WoW wake events, 2 s wakeup hold after a firmware wake | 36.4 | 5 | 2 | 0 |
  | + beacon miss passed to mac80211 at WoW resume, 3 s hold for it | 34.6 | 11 | 3 | 0 |
  | + 12 s hold after a deauth/disassoc wake (13 h, one boot) | 32.3 / 35.9 | 76 | 0 | 0 |
  | 0061 + 0062 as committed (release at STA authorized), core + snoc, 2 h + 4 h + 6 h, one boot | 35.4 / 35.6 / 34.8 | 75 | 0 | 0 |

  - **The keepalive did not stop the AP drops.** The EAPOL wake events fired once in 6 h,
    and no group rekey was logged, so missed rekeys are not shown to be the cause.
  - **What the logs show:**
    - The wake reasons varied with the AP: one run had 38 `AP_ASSOC_LOST` wakes, looping
      every 2-4 s at about 120 mA, because ath10k does not report a WoW beacon miss to
      mac80211. The next run had none.
    - Holding the system awake for a moment after a firmware wake lets the reconnect finish
      first, in 0.24 s.
    - Every remaining race entered suspend exactly when the 2 s hold ran out: wpa_supplicant
      retries an AP only after a 5-10 s ignore-list delay. The next build holds 12 s after a
      deauth or disassoc wake.
    - With the 12 s hold (2026-11-03 16:20 to 11-04 05:22, first 3 h then 10 h overnight):
      no deauth raced suspend and the firmware did not assert once, against 5 asserts in
      7 h on the stock module the night before. Reconnects took 0.25 s (median), the
      slowest 10.24 s. The AP's drops did not stop (76 Reason 7 in 13 h), so the hold
      fixes the crash path, not the drops. The hold still runs its full 12 s after a 0.25 s
      reconnect; the next build releases it once the station is associated.
    - The commits as written (2026-11-05/06, 12 h on battery in three windows): reconnect
      median 0.28 s, slowest 10.4 s; the wakeup source is released at the reconnect (longest
      hold 10.4 s = the slowest reconnect, not 12 s), 312 s held in 13 h. The sleep current
      is the same as without the hold.
- To do with the SIM back: L8 at 640 mV (`k-l8` image, the dtb change only) with
  `CYCLE_DISABLE=1`, first modem crash on 3 fresh boots; one control on the daily DT first
  to confirm the crash still reproduces.
