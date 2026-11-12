# VoLTE on the mainline Pro1-X (prototype, 2026-11-12)

State: **incoming calls over VoLTE worked** in a manual test on Vodafone DE (LTE). Two calls
rang at once, the status bar kept 4G through the calls (before this, every incoming call moved
the phone to 2G), audio both ways (author). Since 2026-11-13 the plumbing is in place:
oFono patch 0022 and the daemon `qx1050-imsd` (unit shipped, **not enabled**:
`systemctl enable --now qx1050-imsd`). Verified: the modem asks, oFono's `ims` context comes up,
the address is reported. Calls through the daemon, outgoing calls, reboot, suspend and SMS over
IMS are untested.

## How VoLTE works here

The modem runs the whole IMS stack itself: SIP, registration, call control, RTP. Call audio
goes through the same ADSP voice session as a circuit-switched call, so the call audio path of
the port needs no change. The host has one job: when the modem's IMS client asks, bring up
the IMS data connection (PDN) and tell the modem its address. On Android a vendor daemon does
that; on mainline Linux, [81voltd](https://gitlab.postmarketos.org/modem/81voltd) (Richard
Acayan, GPL-2.0-or-later) does it through ModemManager, and kabouik showed it working on this
phone under Guix. SailfishOS uses oFono, which cannot run beside ModemManager, so
`tools/volte/qx1050-ims-step3.py` stands in for 81voltd with plain QMI over QRTR.

## What the tests showed (2026-11-12, Vodafone DE, carrier profile Germany-VoLTE-Vodafone)

1. The modem announces its IMS services (QMI 33, 18, 77). As soon as the host publishes the
   IMS data service (QMI 770), the modem's IMS client connects to it.
2. **The modem asks for the IMS PDN only with the voice domain at ps-preferred.** At
   cs-preferred, which `qx1050-modem-prep.sh` sets at every modem start so that calls ring by
   CS fallback, it never asked within 3 minutes. At ps-preferred it asked one second later:
   APN `ims`, IPv6, 3GPP profile 2.
3. **The network grants the IMS PDN only when START NETWORK carries the 3GPP profile index**
   (TLV 0x31). With the APN alone, which is what oFono's qmimodem driver sends, the modem
   returns CallFailed with call manager reason 231; oFono logs "Unknown error type" because it
   does not read the call end reason. IPv4 on profile 2 is refused (reason 208): the IMS PDN
   is IPv6 only.
4. With the PDN up on a free mux and its address reported back, incoming calls rang at once
   and stayed on LTE. The IMSA service keeps answering InvalidOperation to registration
   queries even then (kabouik saw the same), so it is no indicator; the call is.
5. The modem holds 25 carrier profiles: mostly Asian carriers, Germany Vodafone and
   Telefonica, and the generic ROW_Commercial. There is no UK profile; a UK SIM would run on
   ROW_Commercial, which is what worked for kabouik on Free (France). Untested.

## The tools (`tools/volte/`, run as root on the phone)

- `qx1050-ims-probe.py [seconds]`: publishes service 770 and only logs what the modem asks;
  answers Start Connection with an error, so nothing comes up.
- `qx1050-wds-try.py <mux> <4|6> [profile=N] [apn=X] [auth=...]`: one QMI WDS client in one
  process (on QRTR a client lives only as long as its socket, so qmicli cannot bind and start
  across calls). Starts a PDN and prints the call end reasons; stops it again at once.
- `qx1050-ims-step3.py [seconds] [mux]`: the stand-in. Sets ps-preferred (until the next
  modem power cycle), serves 770, brings up the requested PDN with its profile index on the
  given mux (default 1, `rmnet_data0`, unused by the internet context) and reports the
  address; tears it down and sets cs-preferred back on exit. It needs a stdout that stays
  open: if its output pipe breaks, the cleanup can stop before cs-preferred is restored
  (check with `qmicli -d qrtr://0 --nas-get-system-selection-preference`).

## The plumbing (2026-11-13)

- **oFono patch 0022** (`patches/ofono/`): the QMI driver passes the modem's stored profile
  index for the context's APN, fixes the IPv6/IPv4v6 type mix-up of `qmi_activate_primary`
  (an upstream bug, also why a dual-stack internet context failed), logs call end reasons.
- **`qx1050-imsd`** (`/usr/bin/droid/qx1050-imsd.py`, `qx1050-imsd.service`): serves QMI 770;
  on the modem's request finds or creates the oFono context of type `ims`, sets APN and
  protocol as asked, activates it, reports the address; deactivates on the modem's stop; sets
  ps-preferred while running and cs-preferred on stop (also `ExecStopPost`). Watchdog after
  kabouik's: on LTE without an active `ims` context for 30 s it publishes the service again so
  the modem asks again, backing off 90 s doubling to 30 min. Prototype in Python with
  dbus-send (no Python D-Bus bindings on the image); a C daemon (81voltd with an oFono backend)
  is the clean form.
- **`qx1050-modem-prep.sh`** sets ps-preferred instead of cs-preferred after a modem restart
  while `qx1050-imsd` is active.
- The `ims` context lives on its own rmnet link (rmnet_data1 in the test); the IPv6 default
  route stays where it was, connman does not use the context.

## Still to do

1. Tests: incoming and outgoing calls with the daemon (kabouik's outgoing fell back to 3G after
   22-26 s), after reboot with the unit enabled, after suspend, airplane mode, mobile data off,
   SMS while IMS is up, emergency calls stay CS. The QMI call mode (`lte`) per call.
2. A C daemon instead of the Python prototype; a qmimodem `ims` atom for a registration
   indicator later (the IMSA service answers InvalidOperation even with VoLTE up).
3. Upstream: the 0022 IPv6 fix to oFono.

Seen on the way: answering a call logged "Unbalanced enable for IRQ 200" from
`wcd937x_codec_enable_ear_pa` (kernel warning, no effect noticed); for the kernel list.

Credits: Richard Acayan for 81voltd and its message definitions; kabouik (Mathieu) for showing
81voltd on this phone and for the watchdog finding.
