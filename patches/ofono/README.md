# ofono: slot driver for the QRTR modem

`0019-qrtrqmi-add-slot-driver.patch` goes on top of the sailfishos-native/ofono patch series
(github.com/sailfishos-native/ofono, commit 4c79652 "Update to 2.12", the state the
dontbeevil OBS ofono 2.12.0+git1 on the phone was built from). Applies unchanged on 2.13 too.

Without it ofono drives the modem (SIM present, registered) but org.nemomobile.ofono.ModemManager
lists no modems, so Settings shows no SIM and connman/commhistoryd see no modem.

What it does, modelled on patch 0010 (gobi slot support) but without the global modem pointer
and the one-shot SIM sampling: registers a slot driver on probe, adds one slot when DMS GET_IDS
answers (IMEI, placeholder if the modem gives none), and follows the SIM atom through
ofono_watch, so an absent SIM ends up ABSENT instead of keeping the manager unready.

Build: sailfishos-native/ofono at 4c79652, submodule upstream at 2.12, all rpm/*.patch applied
in-tree (mb2 skips %prep), spec Version 2.12.0+git2, Patch18 added. Needs ell-devel 0.70 from
the dontbeevil OBS in the target.

## Device result 2026-10-26 23:15 (7.3.0-qx1050)

- Works: ModemManager lists /qrtrsoc_0 (available, enabled, default voice + data), present SIM,
  ready; Settings shows the mobile network page; connman has a Cellular technology and service.
- Missing: the Settings SIM tab (not investigated yet).
- Mobile data fails before any APN matters: ofono logs `Failed to bind MUX` for all three
  premux contexts (rmnet_data0-2, mux 1-3), so the gprs atom has no context driver ("Can't assign
  context to driver for APN", connman connect -> NotImplemented). APN stays empty
  ("Duplicate detected for 26202, spn: freenet").
- qmicli 1.34 against the modem (MPSS.HA.1.1-00140, 2021): WDS BIND_MUX_DATA_PORT with
  client-type (TLV 0x13, which ofono always sends) -> InvalidArgument; without it -> Internal,
  for every endpoint type/iface/mux tried. WDA GET_DATA_FORMAT -> InvalidArgument for every
  endpoint. Next: find how this firmware expects the data path (ModemManager logs from the
  NixOS side, or the vendor Android rmnet/IPA setup) before changing ofono.

## 0020: IPA data path (mobile data works, 2026-10-27)

`0020-qrtrqmi-ipa-data-path.patch` goes on top of 0019 (spec Version 2.12.0+git4, Patch19).
The sequence was taken from ModemManager 1.24.2 connecting on the same phone and firmware
(NixOS side, debug log): on the embedded bus, before the first bearer,

1. DPM OPEN_PORT (service 47, 0x0020), hardware data port {embedded, iface 1, rx = ipa
   `modem/tx_endpoint_id`, tx = ipa `modem/rx_endpoint_id`} (1 and 9 here). Without it every
   WDS bind fails.
2. WDA SET_DATA_FORMAT: raw-ip, UL/DL aggregation **QMAPv4** when the ipa driver reports
   `feature/rx_offload` MAPv4 (plain QMAP otherwise), DL max 32 datagrams / 32768 bytes,
   endpoint {embedded, 1}. With plain QMAP uplink packets leave but no downlink ever arrives,
   and once QMAP is set the modem keeps it until it restarts.
3. WDS BIND_MUX_DATA_PORT without the client-type TLV 0x13 on embedded endpoints (the
   firmware answers InvalidArgument to it); gprs-context.c change.

The rmnet links need the MAPv4 checksum flags (ingress and egress). iproute2 has no rmnet flags
option; `qx1050-modem-prep.sh` creates them with `qmicli --link-add ... flags=`.

Result on a clean boot (7.3.0-qx1050, freenet on 26202, APN web.vodafone.de from provisioning):
context active, modem reports qmapv4/qmapv4, ping 3/3 at ~40 ms, HTTPS 200 in 0.5 s.
Not checked yet: the Settings mobile data toggle and the browser with Wi-Fi off, IPv6, and
restarting ofono without a modem restart.
