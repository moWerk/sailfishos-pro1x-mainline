# Changes taken from the Guix port (2026-11-08)

Source: mlaparie/fxtec-pro1x-guix (kabouik), `main` 2cc19f1 and `next` d378ec6. Both ports
started from nyymi609's NixOS port; the Guix port had in turn taken this port's kernel series up
to 0057 and its libcamera 0004-0017. Comparison report: private, not in this repo.
Running on pro1x#2 as kernel kb2 (#9 + DTB with 0067).

| Change | Where | Result on pro1x#2 |
|---|---|---|
| Bluetooth LE (`BT_LE`, `BT_LE_L2CAP_ECRED`, `BT_RFCOMM_TTY`, `UHID`) | `kernel/sfos.fragment` | **works**: Amazfish (harbour-amazfish 2.9.0, Chum) bonded two AsteroidOS watches. LE Secure Connections with numeric comparison, the Jolla agent's confirmation dialog, then time sync and notifications. Before, arm64 defconfig left no LE at all |
| GNSS engine unlock | `qx1050-modem-prep.sh` | **works**: time and XTRA injection accepted (`gnss.md`) |
| XTRA through the native provider | `geoclue-provider-hybris-qmi`, `/etc/gps_xtra.ini` | **works indoors**: after deleting all assistance data the provider downloaded 47722 B and the modem reports 166 h. TTFF outdoors not measured yet |
| qrtr modem wake by source port (0066) | kernel, `qx1050-modem-prep.sh` | wake_ports set at each modem start (voice and SMS ports, which move with a modem restart). Effect with a SIM untested |
| awake_charging switch (0063), panel/DPU gap (0064, 0065) | kernel | carried, off or default |
| LED pattern trigger, compressed firmware | `kernel/sfos.fragment` | built, unused |
| libcamera 0018 (VBLANK, longer exposures) | `patches/libcamera` | installed; dark-scene effect not judged yet |
| libcamera 0019 (AGC 3x faster) | `patches/libcamera` | **dropped**: tap-to-focus at 50 cm never focused. Without it 50 cm focuses |
| Speaker HPH Volume 20 | UCM `HiFi.conf`, `VoiceCall.conf` | **works**: a web stream at full volume on the speaker, no clipping (author) |
| In-call mute (VoiceMMode1 capture mixer) | `qx1050-voicecall` 0.1-8 | needs a SIM |
| BT UART and IPA wakeup off | udev rules | both read `disabled` after boot; standby A/B not measured |
| haptics-tune.sh | `tools/` | not run |

Not taken: the keyboard backlight on at boot (only for the Guix port's encryption prompt), the
fingerprint TEE driver (not working there either), keyd Fn layer (SFOS reacts to key events,
not XF86 keysyms; proposal in `research/research-keyboard.md`), VoLTE through 81voltd (needs an
oFono equivalent; a project of its own).

## Open after the hands-on run

- **BLE pairing with the sol watch (2026-11-08, btmon on both sides):** the first attempts
  failed on the watch. The phone's SMP Pairing Request reached sol's controller, but sol's host
  saw it only minutes later: its runtime-suspended BT UART never woke for receive data. After
  the meta-smartwatch session kept that UART awake, sol bonded. Later link drops (supervision
  timeout 27 s after bonding, then 0x3e at a reconnect) are on the watch side too. Phone tool:
  `btmon` from `bluez5-tools` (Jolla repo), `btmon -w file`, then `btmon -T -r file`.

- **Camera AF** (author, 2026-11-08, without 0019): 50 cm focuses; very close macro does not
  trigger AF; the tap-to-focus circle turns red even when the image gets sharp after 2-3 s.
  Not yet known whether these predate 0018. Parked.
- **Headphone jack**: plugging in does not move audio off the speaker; it plays on both,
  because the speaker amplifiers are fed by the HPH stage the Speaker device switches on.
  - The jack controls (`Headphone Jack`, `Mic Jack`) and the `Headset Jack` input device exist,
    but never report a plug.
  - The board DT lacked the switch type. Stock (BENGAL IDP overlay) has
    `qcom,msm-mbhc-hphl-swh = <1>` and `qcom,msm-mbhc-gnd-swh = <1>`. Downstream documents
    1 as "normally open", but it is the same `hphl_swh`/`gnd_swh` bit that mainline sets with
    `qcom,*-jack-type-normally-closed`. 0067 adds those, as the armada-os fix for a WCD939x
    handheld did (armada-os/armada PR 803).
  - With 0067, each plug edge raises the MBHC switch interrupt (2 counts per cycle). Without
    it, nothing counted. But no plug is ever reported, and "Elect Insert" stays 0.
  - Next: a dynamic-debug kernel and `wcd-mbhc-v2.c` debug output during one replug.
  - UCM has `JackControl "Headphone Jack"` on Headphones. Priority 300 (above the speaker's
    200) silenced the phone: with no jack report the port is "availability unknown", so
    PulseAudio picked Headphones, which plays nothing. card-restore kept that profile after
    the next boot, and the default sink became `sink.null`. Back to 100 until plugs report.
  - **A plug during playback stalls the stream** (2026-11-08): the RX codec DMA PCM stays
    RUNNING, but `hw_ptr` stops (0 while `appl_ptr` sits at a full buffer). The speaker
    amplifiers then amplify an idle HPH stage: a constant hiss and no sound from any app.
    No kernel error besides wcd937x "clk already disabled". Recovery without a reboot:
    `pactl suspend-sink alsa_output.Pro1X.HiFi__Speaker__sink 1`, then `0`; the stream
    reopens and plays. The browser segfaults when its stream goes away.
  - Second run the same evening: the plug *was* reported this time (PulseAudio moved to
    "HiFi (Headphones, Mic)", the Headphones sink), sound came from both speaker and
    headphones, and the unplug stalled the stream the same way (Headphones sink RUNNING,
    `hw_ptr` 0). So detection works at least sometimes, the speaker amplifiers do not go
    quiet with `Spk0/Spk1 OUT Switch` 0 on that path, and the stall follows a device switch
    while a stream runs, not the plug itself.

## Charge limit at 80 % "ignored" (2026-11-08)

Not mce: on the power bank mce disabled charging at 18:25 (`mch_charging_state: allowed ->
disabled`, kernel "charging disabled"). The reading then went 84 -> 98 % in five minutes
and mce saw `full`, which no charger can do. After the next boot the gauge read 79 %.

Cause in `pro1x-adc-battery`: with USB in and charging held off, the battery current is near
0, so the OCV anchor branch runs every poll, on a voltage still raised by the charge before.
0036's relaxation guard only covers the time after unplugging (`relaxing` needs `!usb`).
Fix to make: no OCV anchoring while USB is in and the battery is not charging, and the
relaxation window counted from the end of charging, not from the unplug.
