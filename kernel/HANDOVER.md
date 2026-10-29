# Pro1-X kernel series on v7.3: what each patch is, and what it would take upstream

For whoever picks this up. Branch `qx1050-7.3` (see README.md), 16 commits on v7.3.
Status labels: **upstream** (already in mainline, dropped), **upstreamable** (a real
hardware enable, needs the listed work), **SFOS-only** (a workaround for SailfishOS
userspace, fix that userspace instead), **out-of-tree** (someone else's unmerged series).

None of the pro1x-* patches carries a commit message or a Signed-off-by: they come from
fxtec-pro1x-nixos `device/kernel/patches/` (nyymi609) as plain diffs. Sending any of it
upstream needs the original author's sign-off first.

| Commit | Size | What | Status | Missing for upstream | Device-tested |
|---|---|---|---|---|---|
| pro1x-touch | DTS | Goodix GT9286 on an i2c-gpio bus (pins 6/7) instead of i2c2 | upstreamable | reason for i2c-gpio over the GENI controller in the commit message | 7.1: yes (SFOS UI) |
| pro1x-egpio | — | sm6115 pinctrl egpio | **upstream** in 7.3 | — | — |
| pro1x-dsi | panel driver | BOE BF060Y8M-AJ0 pixel clock for exactly 60 Hz (touch ghost events at 61 Hz) | upstreamable, small | measurement in the message | 7.1: yes |
| 0023 panel display off | +18/-3 | BOE BF060Y8M-AJ0: display off from `.disable` while video runs, sleep in stays in unprepare (was: one corrupted frame at full brightness on every blank) | upstreamable, small | — | 7.3: yes, clean fade (2026-10-29) |
| 0024 UFS PM QoS follows clocks | +9/-3 | ufshcd: the CPU latency vote follows clock gating also with clock scaling; scale-down keeps it (QD1 reads 48 -> 86 MB/s) | upstreamable, small; needs a power measurement in the message | — | 7.3: yes (2026-10-29) |
| pro1x-audio | 9 files, +629 | WCD9370 codec, AW87339 speaker amps (new driver + binding), sm8250 machine driver bits, lpasscc | upstreamable | checkpatch, review; the first-probe silence (notes/upstream.md) is unresolved | 7.1: yes (with reprobe workaround) |
| pro1x-q6voice | 17 files, +1967 | q6voice (CVP/CVS/MVM) for in-call audio | **out-of-tree** (Stephan Gerhold's series, MODULE_AUTHOR his) | follow his upstreaming, do not send separately | 7.1: not verified on SFOS (calls untested) |
| pro1x-camera | 8 files, +3510 | IMX582 (rear), S5K4H7 (front), DW9800 VCM drivers, CAMSS sm6115 resources, DTS | upstreamable, the big one | DT bindings for three devices (none in the patch), v4l2-compliance, register tables' provenance (vendor module config), libcamera sensor helpers already exist as patches | 7.1: yes (photos, video, AF) |
| pro1x-keyboard | +622 | AW9523B 8x8 keyboard matrix driver | upstreamable in principle | binding; maintainers may want it as an aw9523 MFD/matrix-keypad user rather than a board-specific driver | 7.1: yes |
| pro1x-leds | +61 | torch / keyboard backlight / qcom flash LED | upstreamable | — (small) | 7.1: yes |
| pro1x-power | +897 | battery estimate from PMIC ADC + charger input limit + watchdog | questionable as is (board-specific estimator, two `pro1x-*` drivers) | a real fuel gauge path (qcom pmi632 ADC/fg) would be the upstream route | 7.1: yes |
| pro1x-haptics | +317 | AW86224 vibration driver | upstreamable | binding | 7.1: yes (with gain 7 / drive 100) |
| pro1x-bluetooth | DTS | WCN3950 | upstreamable | — (the 7.1 re-power failure, notes/upstream.md, did not reproduce on 7.3) | 7.1: works from boot only; 7.3: yes, A2DP, repeated off/on (2026-10-28) |
| pro1x-ipa | DTS | modem data path (IPA) | upstreamable | — | 7.1: modem registers; data partly |
| sfos-dt | DTS | kbd LED active-low (**upstreamable**, real hardware fact), Fn -> KEY_RIGHTALT (**SFOS-only**), slide inverted (**SFOS-only**, mce) | mixed | split: send the LED polarity; fix mce (slide) and the keymap (Fn) instead | 7.1: yes |
| sfos-venus | DTS | `&venus { status = "okay"; }` | upstreamable, 9 lines | SoC node and gcc flag are upstream since 7.2/7.3 | 7.1 backport: yes (H.264 encode) |
| sfos-imx582-1080p | +7/-7 | 1984x1080 mode for libcamera's swISP border | arguable | maybe fix libcamera instead; upstream would take a correct 1080p mode | 7.1: yes |
| sfos-dw9800-soft | +34/-12 | quiet lens ramps, autosuspend, real park position | upstreamable fix to the dw9800 driver once that driver is upstream | — | 7.1: booted, clack test pending |
| IIO_CONSUMER import | +1 | 7.2+ namespace for pro1x-adc-battery | fold into pro1x-power | — | build only |

## Not in the kernel, but where the easy wins are (SFOS packaging)

libcamera 0.7.2 with the AF series (port/patches/libcamera), qtmultimedia camerabin fixes
(port/patches/qtmultimedia), gst-plugins-good v4l2-probe, protobuf-c / libssc / hexagonrpc
(port/packages), ngfd and pulseaudio items: see notes/upstream.md and
notes/external-packages.md. Most of it helps every native (non-hybris) SailfishOS port, not
only the Pro1-X.
