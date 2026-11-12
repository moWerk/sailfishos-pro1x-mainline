# VoLTE on the mainline Pro1-X (prototype, 2026-11-12)

State: **incoming calls over VoLTE worked** in a manual test on Vodafone DE (LTE). Two calls
rang at once, the status bar kept 4G through the calls (before this, every incoming call moved
the phone to 2G), audio both ways (author). Nothing is installed or automatic yet: the
stand-in runs by hand. Outgoing calls, reboot, suspend and SMS over IMS are untested.

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

## To make it real

1. oFono: pass the 3GPP profile index for an `ims` context in START NETWORK (a small patch to
   `drivers/qmimodem/gprs-context.c`), and read the call end reason into the error.
2. An IMS data daemon: 81voltd's QRTR side with an oFono backend instead of ModemManager
   (an `ims` context per request), or a small daemon of our own; systemd unit after ofono.
3. Voice domain: ps-preferred only while that daemon runs; cs-preferred back on stop.
4. kabouik's watchdog: after a call that fell back to 2G/3G the modem may close the IMS PDN
   and not ask again on LTE; restart the request when on LTE without it (back off for
   networks without VoLTE).
5. Tests: outgoing calls (kabouik's fell back to 3G after 22-26 s), after reboot, after
   suspend, airplane mode, SMS while IMS is up, emergency calls stay CS.

Seen on the way: answering a call logged "Unbalanced enable for IRQ 200" from
`wcd937x_codec_enable_ear_pa` (kernel warning, no effect noticed); for the kernel list.

Credits: Richard Acayan for 81voltd and its message definitions; kabouik (Mathieu) for showing
81voltd on this phone and for the watchdog finding.
