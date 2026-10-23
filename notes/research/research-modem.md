# Modem on SFOS 5.1 / mainline Pro1-X (SM6115, QRTR): research, 2026-10-23

[C] confirmed (URL/path + snippet seen), [I] inferred. Read-only; no device touched. Local copies in scratchpad/:
sfos-ofono (sailfishos/ofono master), upstream-ofono (2.19), msm-modem-13, nixos-pro1x (nyymi609), sfn-patches.

## 1. ofono: Jolla fork vs upstream, QRTR, UIM provisioning
- [C] Jolla fork = ofono 1.29 (source in `ofono/` of github.com/sailfishos/ofono; SFOS 5.1 ships 1.29+git12).
  `drivers/qmimodem/` exists but `grep -rli qrtr` = 0 hits; `plugins/gobi.c` does `open(device)` +
  `qmi_device_new(fd)` = QMUX over USB cdc-wdm only. `rpm/ofono.spec:92 --disable-qmimodem` (commit "Disable
  PhoNet/ISI and QMI modem support. MER#1734"). SFOS extensions live in src/: slot-manager-dbus.c
  (org.nemomobile.ofono.ModemManager), sim-info-dbus.c, cell-info-dbus.c, dbus-access.c.
- [C] Upstream (git.kernel.org, now 2.19) ChangeLog: 2.5 "QRTR services", 2.6 "QMI voice call feature",
  2.9 "QRTR based devices (MHI bus and SoC)", 2.13 QMI PIN/Lock, 2.18 QMI SMS-ack fix. `plugins/qrtrqmi.c` is
  built whenever qmimodem is enabled (Makefile.am:395) and registers as `OFONO_MODEM_DRIVER_BUILTIN(qrtrqmi)`;
  the qmimodem atoms are `OFONO_ATOM_DRIVER_BUILTIN` (not plugins, so `--plugin/--noplugin` cannot drop them);
  provisioning is core (`src/provision.c` + generated `provision.db`, no mobile-broadband-provider-info).
  Needs ell >= 0.79 (`--enable-external-ell`) or the bundled ell tree.
- [C] UIM provisioning: `drivers/qmimodem/uim.h` only defines session types (PGWP=0x00 ...); `sim.c` uses CS1
  for PIN ops; no CHANGE_PROVISIONING_SESSION (0x38) anywhere in drivers/qmimodem. No ofono does the fix itself.
- [C] Detection requirements (upstream `plugins/udevng.c`): `check_net_device()` builds a "qrtrsoc" modem only
  from netdevs named `rmnet_*`, grouped by iflink; `setup_qrtrsoc()` needs one `rmnet_ipa*` AND >= 3 existing
  `rmnet_dataN` (`if (premux_count < 3) return FALSE`; mux_id = N+1). `qrtrqmi_probe()` needs Bus=embedded;
  `lookup_done()` needs QRTR services DMS, UIM, WDS, NAS. Debug: env `OFONO_QMI_DEBUG`.
- [C] PinePhone SFOS port runs upstream-2.x ofono under the 5.x stack: OBS nemo:devel:hw:pine:dontbeevil ships
  `ofono-2.12.0+git1`, `ell`, `eg25-manager` (no libqmi); ref/droid-config-pinephone
  `sparse/var/lib/environment/ofono/noplugin.conf` = `--noplugin=<list> --plugin=udev,udevng,qmimodem,gobi,
  sailfish_manager` (1.x-era names). Older sailfish-on-dontbeevil/ofono (1.21 + sysmocom voice patches) superseded.
- [C] github.com/sailfishos-native/ofono: upstream 2.13 + 17 patches: 0003 sailfish-access, 0004 cell-info,
  0005 pushforwarder, 0007 sim-info, 0008 slot manager + org.nemomobile.ofono.ModemManager (3013 lines, core
  `OFONO_MODULE(slot_manager)`, configure `--enable-sailfish-slot`), 0010 "Add slot support to the gobi plugin"
  (274 lines, gobi.c only, hardcoded IMEI placeholder), 0011 ofono.service `EnvironmentFile=-/var/lib/environment/
  ofono/*.conf` + `ExecStart=ofonod -n $OFONO_ARGS $OFONO_DEBUG`, 0015/0016 wakelocks. Spec: `--enable-qmimodem
  --enable-external-ell --disable-rilmodem`, `Requires: ell` (sailfishos-native/ell packaged). Last push 2024-11.
  => nobody has wired qrtrqmi into the slot manager yet [I]. affe_null's C2 mainline ofono is a sprdmodem driver
  on the 1.29 fork (Unisoc), not applicable.

## 2. ModemManager on SFOS; what pmOS/Nura does
- [C] No ModemManager/libqmi/libqrtr-glib/msm-modem in SailfishOS:Chum 4.6/5.0/5.1 listings, pine OBS repo, or
  the Jolla rootfs package list (port/notes/stage1.log). droidian/ofono2mm = MM API on top of ofono (wrong
  direction). [I] No ofono-on-MM bridge exists; SFOS consumes ofono only (sect. 3) => MM is a dead end here.
- [C] pmOS/Nura `msm-modem` v13 (gitlab.postmarketos.org/postmarketOS/msm-modem; Alpine community/msm-modem):
  subpackage `msm-modem-uim-selection` depends `rmtfs libqmi qmi-utils`. Script picks `/dev/modem`,
  `/dev/wwan0qmi0` or `qrtr://0` (`qmicli --silent -pd qrtr://0 --uim-noop`), waits SIM_WAIT_TIME (4 s) for
  "Card state: 'present'", if a Primary GW session exists runs `--uim-change-provisioning-session='activate=no,
  session-type=primary-gw-provisioning'`, then first present slot + first `usim (2)` AID:
  `--uim-change-provisioning-session="slot=$S,activate=yes,session-type=primary-gw-provisioning,aid=$AID"`.
  Unit: `After=/Requires=rmtfs.service`, `Before=ModemManager.service`, `WantedBy=multi-user.target`,
  `RequiredBy=ModemManager.service`, Type=notify; rmtfs drop-in `Before=msm-modem-uim-selection.service`.
  => runs on EVERY boot by design (idempotent), does not rely on persistence. The NixOS reference
  (nyymi609 modules/modem.nix) has no such unit: only tqftpserv + `rmtfs -r -P -s` + MM — consistent with your
  one-time fix. [I] The session is stored in modem EFS (modemst1/2 via rmtfs); SFOS's rmtfs serves the same
  partitions, so it is probably already active. Test before packaging libqmi.
- [-] Nura device-fxtec-qx1050 (MR !5350, testing, kernel 6.10) not fetchable (Anubis/404); modem not claimed.

## 3. What the SFOS stack expects from ofono
- [C] Rootfs has ofono, libqofono-qt5, libqofonoext, libgofono, libgofonoext, connman, commhistory-daemon,
  telepathy-ring, voicecall-qt5, mms-engine, simkit, jolla-settings-networking-cellular. connman spec:
  `Requires libgofono`, `BuildRequires pkgconfig(libgofonoext)`, `--enable-sailfish-ofono`; commhistory-daemon
  `BuildRequires libqofonoext-devel`; voicecall: libqofono only; libgofonoext = org.nemomobile.ofono.ModemManager.
- [I] Standard org.ofono.* (Modem, SimManager, NetworkRegistration, ConnectionManager, VoiceCallManager,
  MessageManager) come from upstream core, but connman's cellular side, commhistoryd and the SIM/slot UI go
  through org.nemomobile.ofono.ModemManager/SimInfo = patch 0008's slot manager + a slot driver. Plain upstream
  ofono = test-script-level function, no mobile data via connman, no SIM in Settings. No forum/list report of
  plain upstream ofono on SFOS found; the PinePhone path is the only precedent.

## 4. Kernel/data path for this device
- [C] Upstream `sm6115.dtsi` (7.1) has no IPA and no bam-dmux node. NixOS reference carries
  `device/kernel/patches/pro1x-ipa.patch` (`ipa@5840000`, `compatible = "qcom,qcm2290-ipa"`; comment: "ModemManager
  needs the network interface it creates, even for SMS and calls") and builds ipa.ko + rmnet.ko (qrtr builtin).
  MM's 77-mm-qcom-soc.rules keys on `DRIVERS=="ipa"` too. No `rmnet_ipa0` => no modem object for MM or ofono.

## Recommendation: least-effort path to calls/SMS/data
sailfishos-native/ofono (upstream 2.13 + SFOS patches) with qrtrqmi; not Jolla's 1.29, not ModemManager.
1. Verify on device first: `qrtr-lookup` lists DMS(2) NAS(3) WMS(5) WDS(1) UIM(11) WDA(26) VOICE(9);
   `ip link` shows `rmnet_ipa0` (else port pro1x-ipa.patch into the SFOS kernel first);
   `qmicli -d qrtr://0 --uim-get-card-status` shows a Primary GW session (if "session doesn't exist" -> step 6).
2. Package ell (sailfishos-native/ell) and ofono from sailfishos-native/ofono as-is; drop Jolla ofono/ril-binder
   from the qx1050 patterns. Rebase to 2.19 later (QMI fixes 2.14-2.18).
3. Config package: `/var/lib/environment/ofono/qx1050.conf` with a deny-list only, e.g.
   `OFONO_ARGS="--noplugin=bluez5,hfp_bluez5,hfp_ag_bluez5,dun_gw_bluez5,phonesim,..."` — keep `udevng` loadable
   (it is the only plugin in this path; qrtrqmi/qmimodem/slot manager/provisioning are builtin in 2.x), so do NOT
   copy the PinePhone `--plugin=` allow-list. Plus a oneshot `After=rmtfs.service Before=ofono.service` that waits
   for the IPA netdev (it appears only after the modem remoteproc is up) and creates the premux links udevng insists on:
   `for i in $(seq 1 180); do [ -e /sys/class/net/rmnet_ipa0 ] && break; sleep 1; done;`
   `for n in 0 1 2; do ip link add link rmnet_ipa0 name rmnet_data$n type rmnet mux_id $((n+1)); done`.
   ofono.service `After=rmtfs.service tqftpserv.service`.
4. The one real coding job: port patch 0010 (gobi slot shim) to plugins/qrtrqmi.c (~270 lines, mechanical:
   ofono_slot_driver_register, one slot with the modem path, SIM presence from the sim atom; take the IMEI from
   DMS GET_IDS instead of the placeholder). Without it the SFOS UI shows no SIM and connman gets no modem.
5. Test order: SimManager Present -> NetworkRegistration -> SMS (test/send-sms) -> data (connman cellular;
   APN from ofono's provision.db) -> voice. Call audio is separate (NixOS: q6voice driver + UCM "Voice Call" verb;
   SFOS: pulseaudio/ohm routing), not covered here.
6. Only if provisioning did not persist: package libqrtr-glib + libqmi and ship the pmOS script as
   `msm-modem-uim-selection.service` (`After=rmtfs Before=ofono`), or add PGWP activation (UIM 0x38) to qmimodem
   sim.c — small and upstreamable.
