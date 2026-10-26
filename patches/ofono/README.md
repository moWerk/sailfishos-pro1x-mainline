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
