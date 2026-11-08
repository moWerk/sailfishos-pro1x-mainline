# Changes taken from the Guix port (2026-11-08)

Source: mlaparie/fxtec-pro1x-guix (kabouik), `main` 2cc19f1 and `next` d378ec6. Both ports
started from nyymi609's NixOS port; the Guix port had in turn taken this port's kernel series up
to 0057 and its libcamera 0004-0017. Comparison report: private, not in this repo.
Running on pro1x#2 as kernel kb2 (#9 + DTB with 0067).

| Change | Where | Result on pro1x#2 |
|---|---|---|
| Bluetooth LE (`BT_LE`, `BT_LE_L2CAP_ECRED`, `BT_RFCOMM_TTY`, `UHID`) | `kernel/sfos.fragment` | **works**: Amazfish (harbour-amazfish 2.9.0, Chum) found and paired a BLE watch. Before, arm64 defconfig left no LE at all |
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
  - UCM is ready for it: Headphones has `JackControl "Headphone Jack"` and priority 300,
    above the speaker's 200.
