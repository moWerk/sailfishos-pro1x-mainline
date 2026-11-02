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
