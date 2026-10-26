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
