# Pro1-X mainline (SFOS 5.1, Linux 7.1): vibration / ngfd ENOSPC — research report (2026-10-23)

Sources downloaded to scratchpad/src/ (plugin.c, ffmemless.c, core.c = sailfishos/ngfd master 3372625 2025-11-28;
ff-core.c, ff-memless.c = torvalds/linux master; qfeedback.cpp = sailfishos/qt-mobility-haptics-ffmemless).
Nothing on the device was touched. Chip note: the Pro1-X haptics IC is **AW86224** (nyymi's driver, not aw8697/aw86927).

## 1. Why ngfd gets ENOSPC — CONFIRMED (code), root cause: too many effects for 16 memless slots

- Kernel: `input_ff_create_memless()` creates exactly `FF_MEMLESS_EFFECTS` = **16** slots (ff-memless.c:27, :537).
  `input_ff_upload()` returns **-ENOSPC only** when `effect->id == -1` and no free `effect_owners[]` slot exists
  (ff-core.c:124-130). Slots are a single pool per device shared by *all* open fds; freed on EVIOCRMFF or fd close
  (input_ff_flush). FF_GAIN/FF_AUTOCENTER are EV_FF events, not slots — irrelevant here. FF_PERIODIC is emulated:
  memless sets FF_PERIODIC/SINE/TRIANGLE/SQUARE when FF_RUMBLE is set (ff-memless.c:548-554; the AW86224 patch does
  `input_set_capability(EV_FF, FF_RUMBLE)` *before* `input_ff_create_memless`, pro1x-haptics.patch:315-317, so periodic
  effects in ini files are fine and ngfd's device search, which requires FF_PERIODIC too, succeeds — ffmemless.c:112-114).
- ngfd ffmemless plugin init (plugin.c:637-668): uploads 1 "default" rumble effect (ffm_setup_default_effect) + one
  effect per name in `supported_effects` that has a `_TYPE` (ffm_setup_effects, every upload with `ff.id = -1`,
  line 354/483). The ngfd-internal list "must pass": a single failed EVIOCSFF → "Could not load ngfd effects" →
  sink init fails → "Initialization failed". That is exactly the logged sequence.
- Stock `/usr/share/ngfd/plugins.d/50-ffmemless.ini` (list introduced by commit fe30035, Tomi Leppänen 2021-09-10, SFOS 4.3
  era; unchanged in master 2025-11-28 / newest tag 1.4.13 — the exact ngfd build on 5.1.0.11 was NOT inspected, verify
  with the grep in step 3):
  `supported_effects = touch;touch_weak;touch_strong;release;release_weak;release_strong;drag_start;drag_fail;drag_boundary_drag_end;short;strong;long;notice;message;attention;alarm;ringtone`
  = 17 names, types: 10 rumble (touch*, release*, drag_*, short, strong) + 6 periodic (long, notice, message, attention,
  alarm, ringtone); `drag_boundary_drag_end` is an upstream typo (missing `;`) with no `_TYPE`, so it is skipped →
  **16 uploads + 1 default = 17 > 16 → the 17th EVIOCSFF fails with ENOSPC**. Happens on every bare ff-memless device.
  (Inferred, driver not read: Jolla's hybris devices use Qualcomm haptics drivers with their own larger
  `input_ff_create()` pool — the plugin even special-cases FF_CONSTANT for them, plugin.c:262-266 — so Jolla never sees it.)
- Why your NGF_FFMEMLESS_SETTINGS experiments could not help: the env file (sys_props) is only used to *override
  parameters* (plugin.c:664, "ok to fail"); the effect list is read from ngfd_props only (plugin.c:646). And ngfd core
  only merges plugin params from files whose name ends in `ffmemless.ini` (core.c:150-162 `g_str_has_suffix(..., "ffmemless.ini")`),
  sorted with g_strcmp0, later files override earlier keys (core.c:117, :210-217). `ngf-vibra-qx1050.ini` /
  `ngf-vibra-dontbeevil.ini` are never read as plugin params. So "~7 effects uploaded" was never true: it was 17.
- Upstream confirmation: sailfishos/ngfd PR #3 (jmlich, 2021-09-24, PinePhone gpio vibramotor, identical message
  "Vibra upload effect: No space left on device", ngfd then segfaulted — crash later fixed by PR #14, merged 2024-12-04).
  Jolla's Tomin1 reply (2021-10-05): "you could have also just shipped another configuration file which overrides
  `supported_effects` with a shorter list ... named so that it's loaded after 50-ffmemless.conf". PR closed unmerged 2023-01-11.
  https://github.com/sailfishos/ngfd/pull/3

## 2. What other mainline ports do — CONFIRMED from repos

- PinePhone (sailfish-on-dontbeevil/droid-config-pinephone, 5.1 pattern, 2026-05): kernel gpio-vibra (memless, FF_RUMBLE,
  16 slots — same as ours). Ships `var/lib/environment/nemo/60-dontbeevil-vibra.conf` (NGF_FFMEMLESS_SETTINGS +
  FF_MEMLESS_SETTINGS) and `usr/share/ngfd/plugins.d/ngf-vibra-dontbeevil.ini` (old 7-name NGF_SHORT.. list, mix of
  rumble+periodic) + `qtfeedback-dontbeevil.ini`. Pattern requires only `libngf-qt5-qtfeedback`, **not**
  qt5-feedback-haptics-ffmemless. Per the code above that ini is *not* a plugin-param file, and the local clone (HEAD
  26ba6f3) has no `*ffmemless.ini` override. Whether their kernel package patches FF_MEMLESS_EFFECTS was NOT checked
  (GitHub API rate limit hit mid-run; those tree queries returned empty and are void).
  Inferred: PinePhone on 5.x hits the same ENOSPC unless fixed outside droid-config — do not copy it blindly.
- PineTab (droid-config-pinetab pattern, raw fetch OK): requires qt5-feedback-haptics-ffmemless + libngf-qt5-qtfeedback.
- Victara (VerdandiTeam/droid-config-victara, local ref ad2c2db): pattern requires qt5-feedback-haptics-ffmemless +
  libngf-qt5-qtfeedback, `kernel-adaptation-victara`; no ngfd/feedback files at all. His mainline tree
  (VerdandiTeam/victara-mainline-linux, qcom-msm8974-6.16.y) contains pm8xxx-vibrator/gpio-vibra/pwm-vibra (all memless,
  16 slots) — but which one his DTS uses and whether vibration works there is unverified (API rate-limited). Ask Michał.
- Librem 5 / Xperia 10 mainline SFOS: no config repos found (sailfishos-native org has no device configs).
- postmarketOS qx1050 (pmaports MR !5350): no haptics. nyymi's fxtec-pro1x-nixos: AW86224 driver + feedbackd (udev
  FEEDBACKD_TYPE on input "aw86224-haptics"); feedbackd uses FF_RUMBLE directly, uploads per event — no slot pressure.
- HADK middleware page: "The qt5-feedback-haptics-ffmemless used before Sailfish OS 4.3 is deprecated in favor of the
  ... QtFeedback plugin [libngf-qt5-qtfeedback]"; ngfd ffmemless is "the main plugin handling vibra feedback".

## 3. Qt feedback ffmemless plugin — CONFIRMED (qfeedback.cpp)

- Needs only FF_RUMBLE; FF_PERIODIC optional (uploads a 3rd periodic effect if present, :331-345). Per process it uploads
  2-3 effects **into the same 16-slot pool** (theme rumble, custom rumble, periodic).
- `pluginPriority()` = PluginNormalPriority when a device is found (:427); libngf-qt5-qtfeedback = PluginLowPriority
  (ngffeedback.cpp:139). So while qt5-feedback-haptics-ffmemless is installed, lipstick/maliit theme effects (keyboard taps)
  bypass ngfd and go through this plugin.
- Quirk (:398-407): if an upload returns `id == 0` the plugin assumes "effects flushed", re-inits (guard returns false)
  and marks that effect unavailable. With ngfd dead (its fd closed → pool empty) the keyboard process is first in, gets id 0
  for the theme effect → `m_themeEffectsPossible = false` → **no tap vibration even though the plugin "loaded fine"**.
  Inferred as the mechanism; matches your symptom.
- ini keys (FF_MEMLESS_SETTINGS env or build-time file): ACTUATOR_SPIN_UP/DOWN = ms added to every effect length/delay
  (motor latency); ACTUATOR_RUMBLE_MIN/MAX = weak/strong magnitude bounds for custom rumble; ACTUATOR_MAGNITUDE_MAX/MEAN =
  periodic magnitude/offset; {KEYPAD,BUTTON,LONG}_PRESS_{DURATION,DELAY,MAX,MIN} = theme-effect length/delay/strong/weak
  magnitudes; KEYPAD_USE_PERIODIC + KEYPAD_PERIODIC_* = use a periodic waveform for key presses.

## Recommendation (what to change in droid-config-qx1050)

1. Add `sparse/usr/share/ngfd/plugins.d/60-qx1050-ffmemless.ini` (name MUST end in `ffmemless.ini`, sort after 50-):
   ```
   [ffmemless]
   supported_effects = touch;touch_strong;release;release_strong;short;strong;long;notice;message;attention;alarm;ringtone
   ```
   12 names + default = 13 slots, 3 spare. Names not listed (touch_weak, release_weak, drag_*) fall back to "default"
   (plugin.c:698-699); stay <= 15 in any case. Put the AW86224 parameter overrides (your all-rumble values, or copy
   50-ffmemless's periodic ones — emulation works) in the same file; `NGF_FFMEMLESS_SETTINGS`/`ngf-vibra-qx1050.ini`
   then become unnecessary (harmless to keep).
2. Prerequisite for the budget above, not optional: drop `qt5-feedback-haptics-ffmemless` (+ FF_MEMLESS_SETTINGS and
   qtfeedback-qx1050.ini) from the pattern, keep `libngf-qt5-qtfeedback` — as PinePhone does and HADK says. lipstick and
   maliit-server are separate processes, each uploading 2-3 effects into the same pool (13 + 6 > 16). Removing it also
   routes keyboard taps via ngfd (profile vibration level then applies). If you insist on keeping it, cut the ngfd list
   to <= 9 names.
3. Verify on device: `grep supported_effects /usr/share/ngfd/plugins.d/*ffmemless.ini`; `systemctl --user stop ngfd`
   (two instances would compete for the pool), then `ngfd -vvv` as the session user — expect "Created effect ... id N"
   with N <= 15 and no ENOSPC. `evdev_trace -i` (mce-tools) shows the EV_FF caps.
4. Optional kernel alternative (not what other ports do): bump `FF_MEMLESS_EFFECTS` 16→32 in drivers/input/ff-memless.c
   (one line) makes stock ngfd work unmodified; config fix is cleaner and survives kernel updates.
5. Upstream-worthy: ngfd 50-ffmemless.ini typo `drag_boundary_drag_end` (two effects silently replaced by the 240 ms
   default rumble on every device) and ENOSPC not being survivable in ffm_setup_effects. Small PR + forum note.
