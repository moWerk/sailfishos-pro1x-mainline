# Pro1-X mainline SFOS: keyboard backlight (MCE) and QWERTZ layout — research, 2026-10-23

Read-only clones in the scratchpad: sailfishos/mce (tags to 1.119.1), sailfish-on-fxtecpro1/{droid-config-t5,droid-config-halium-qx1050,
xkeyboard-config-pro1}, sailfishos/lipstick, qt/qtbase 5.6 evdevkeyboard. Local: ~/sailfish/work/upgrade/c2-dup-5.1.0.11.log,
/mnt/storage/sailfish/pro1x-mainline/{fxtec-pro1x-nixos,port,nix}. "confirmed" = read in source; "inferred" = reasoning from it.

## 1. How MCE's keyboard backlight works on SFOS 5.1.0.11

- confirmed: 5.1.0.11 ships **mce 1.116.5** (c2-dup log: mce-1.116.5-1.24.1.jolla). `modules/keypad.c` at 1.116.0 has the same md5 as
  1.115.7 and is untouched through 1.118.1; the keypad rewrite (3ec094c/ca812dc, 2026-10-03/04) is 1.119.x only. All below = OLD code.
- confirmed config: section `[KeyPad]`, keys `BrightnessDirectory` (keypad.h:66; list; dir must have `brightness` W_OK and
  `max_brightness` R_OK, old keypad.c:218-234,250-260), `BacklightTimeout` (30 s default), `BacklightFadeInTime/OutTime`. NOT on the
  device: `Type=`, `BinaryBrightnessFile`, `EnableBacklightWhenCharging`, dconf `/system/osso/dsm/keypadbacklight/enabled`,
  `mcetool --set-keypad-backlight[-mode]` (1.119 only; `git show 1.116.5:tools/mcetool.c | grep keypad-backlight` is empty).
- confirmed: mcetool "Keyboard backlight: disabled" = `xmce_get_keyboard_backlight_state` -> D-Bus `MCE_KEY_BACKLIGHT_STATE_GET` ->
  `key_backlight_is_enabled` = `(new_brightness != 0)` at the OUTPUT of `key_backlight_brightness_pipe` (keypad.c:515). It is a readout
  of the current LED state after filtering, not a setting. There is nothing to enable; it is true only while the light is on.
- confirmed policy (keypad.c:595-650, 715-790): on when slide == COVER_OPEN AND system_state == USER (dsme) AND display ON; triggers:
  display->ON, slide-open (ignored if tklocked at that moment), user activity; off on dim/off/`BacklightTimeout`.
- confirmed, most likely root cause: ALS filter. `enable_key_backlight` pushes `max_brightness` into the pipe (keypad.c:601-608); the LED
  is a plain GPIO (pro1x-leds.patch: "plain on/off GPIO (no PWM)"), so leds-gpio gives max_brightness=1. filter-brightness-als.c@1.116.5
  :1307-1321 returns `value * scale / 100`, scale from `[BrightnessKeypad] LevelsProfile0=30..100` (20als-defaults.ini:81-84) whenever
  als_autobrightness is on and a lux value exists: 1*scale/100 == 0 below 3000 lux -> written 0 -> "disabled" although the policy
  fired. With no lux source (`fba_inputflt_output_lux < 0`) scale=100 and 1 is written. Whether sensorfw delivers lux on the mainline
  port is not checked; `mcetool --set-als-autobrightness=disabled` discriminates "policy never fired" from "filter zeroed it".
- confirmed slide source: evdev, not /sys. `SW_KEYPAD_SLIDE` from any KEYBOARD/VOLKEY/INPUT device feeds `keyboard_slide_state_pipe`
  (event-input.c:2404-2409, initial state 2705-2709). event-switches.h:38 `/sys/devices/platform/gpio-switch/slide/state` is N900-era.
- confirmed: `[SW_KEYPAD_SLIDE]` keys are the keyboard's evdev *name* (event-input.c:1939), used only by keyboard_state_update
  (2795-2868) = `keyboard_available` = whether lipstick hides the VKB. KEYBOARD means has KEY_Q and KEY_P (1419-1424). Mainline name is
  "Pro1X keyboard" (pro1x-keyboard.patch:608); the port's "Fxtec Pro1X" matches nothing (hybris: "Fxtec Pro1"). Effect: "non-sliding
  keyboard" -> always available -> VKB suppressed with the slide closed. gpio-keys node has no label -> input name "gpio-keys" (inferred).
- confirmed: probe runs once at module init (keypad.c:889-935); mce.service is Before=basic.target. SFOS kernel has CONFIG_LEDS_GPIO=y,
  CONFIG_KEYBOARD_GPIO=y (nix/store/...-config.sfos.aarch64:8227,3648), so the LED exists before mce; no ordering issue expected.
- confirmed: /etc/mce/[0-9][0-9]*.ini are glob()-sorted and later keys override (mce-conf.c:322-401, 459-480); als-defaults installs as
  20als-defaults.ini (Makefile:495), so a 91-file wins. Profile loader only requires equal Limits/Levels counts (old als.c:987-1034).

## 2. What the hybris ports ship (authors)

- confirmed droid-config-t5 (Pro1): `91-keyboard-backlight.ini` `[KeyPad] BrightnessDirectory=/sys/class/leds/keyboard-backlight`
  (f3a368f 2020-01-23, **Mathieu Laparie / Kabouik** <matf@disr.it>); `90evdev.ini` `[SW_KEYPAD_SLIDE] Fxtec Pro1=gpio-keys` +
  `[EVDEV] KEY_LEFTMETA=KEY_HOME` (piggz 6f414ee 2019-07-08, 7297042 2020-11-02).
- confirmed droid-config-halium-qx1050 (Pro1-X): `BrightnessDirectory=/sys/class/leds/:kbd_backlight/` (486eb5c + cd08a12, Aug 2022,
  author "M" = matf@disr.it = Kabouik: "Enable vkb and hwkb backlight when using Piggz's kernel instead of UT's kernel"); `90evdev.ini`
  `[EVDEV] KEY_HOMEPAGE=KEY_HOME` + `[SW_KEYPAD_SLIDE] Fxtec Pro1=gpio-keys`. No keymap/xkb/udev input files (70-pro1x.rules = Android perms).
- confirmed: our two ini comments credit TheKit/NotKit: wrong. TheKit/NotKit forked xkeyboard-config-pro1 (4622de1, 2019-10-08);
  Michal Szczepaniak (Mister Magister) added de/ru/us-intl layouts (e741802, 2020-01-20); Kabouik sanitized them (7cf0249).

## 3. Keyboard layout

- confirmed hybris Pro1: `xkeyboard-config-pro1` = fork of xkeyboard-config 2.10.1 (Provides/Obsoletes the stock package) adding
  `symbols/fxtec_vndr/pro1` (de/ru/us/us2/pl, all `include "level3(ralt_switch)"` -> the yellow key arrives as KEY_RIGHTALT on hybris) and
  model `fxtecpro1`. Selected via dconf `/desktop/lipstick-jolla-home/layout|options|model` (the file's own comments). The Pro1-X hybris
  port has the package commented out (patterns ...-halium-qx1050.inc:47): it ships NO layout.
- confirmed: lipstick `LipstickKeymap` exposes rules/model/layout/variant/options (lipstickkeymap.h:26-30) -> QWaylandKeymap, so Wayland
  clients get the xkb map. dconf binding of `variant` is inferred (layout/model/options proven).
- confirmed mainline DT (pro1x-keyboard.patch): matrix = letters/digits + KEY_RIGHTALT at (0,3); gpio "Keyboard extra keys" = KEY_HOMEPAGE,
  2x **KEY_FN** ("Super Left/Right"), KEY_RIGHTSHIFT, KEY_LEFTCTRL, KEY_LEFTALT. NixOS: keyd `fn = layer(fn)` with the QWERTZ legends
  (hosts/mo-qwertz/keyd.conf), xkb de/nodeadkeys. So mainline's Fn layer sits on KEY_FN, a different key from hybris' RIGHTALT.
- confirmed: KEY_FN=464 -> xkb `<I472>` exists in xkeyboard-config 2.37 (keycodes/evdev:423; SFOS 5.0 ships 2.37), absent in the 2.10.1 fork.
- confirmed blocker for the KEY_FN route: SFOS Qt is 5.6.3 without libinput (sailfishos/qtbase mer-5.6 spec: only plugin-generic-evdev),
  so lipstick's keyboard input is Qt's evdevkeyboard handler. qtbase 5.6 qevdevkeyboardhandler.cpp `processKeycode`: `if (!it) {
  // we couldn't even find a plain mapping ... return result; }` = unmapped scancodes produce NO event, and the default qmap has no
  0x1d0. Hybris confs pass `-plugin evdevkeyboard[:keymap=droid.qmap]` explicitly; eglfs without LIPSTICK_OPTIONS auto-creates the same
  evdev handlers (inferred). Hence KEY_FN never reaches a client unless a custom qmap maps 0x1d0 (keycode field is quint16, so possible).
- postmarketOS device-fxtec-pro1x: NOT retrieved (gitlab.postmarketos.org raw/API and wiki behind Anubis; gitlab.com mirror 404).

## Recommendation

MCE, `sparse/etc/mce/` (fix both TheKit/NotKit comments to Kabouik / Mathieu Laparie):
```
# 90-qx1050-evdev.ini
[EVDEV]
KEY_HOMEPAGE=KEY_HOME
[SW_KEYPAD_SLIDE]
Pro1X keyboard=gpio-keys          # evdev NAME of the mainline keyboard; verify both names in /proc/bus/input/devices
# 91-qx1050-keyboard-backlight.ini
[KeyPad]
BrightnessDirectory=/sys/class/leds/white:kbd_backlight
BacklightTimeout=30
[BrightnessKeypad]                 # GPIO LED: max_brightness=1; stop the ALS filter scaling 1 down to 0 (overrides 20als-defaults.ini)
LimitsProfile0=1;2;3;6;11;20;36;66;121;220;489;599;732;896;1095;1340;1639;2005;2453;3000
LevelsProfile0=100;100;100;100;100;100;100;100;100;100;100;100;100;100;100;100;100;100;100;100
```
Test without rebuild: `mcetool --set-als-autobrightness=disabled`, slide open, tap screen, within 30 s `mcetool | grep -E 'Display state|
Keyboard backlight'`; `cat /sys/class/leds/white:kbd_backlight/brightness`. If still 0: `mcetool --set-verbosity=debug; journalctl -fu mce`
or foreground `mce --verbose --log-function='modules/keypad.c:*' --log-function='mce-io.c:*'` (flags confirmed in 1.116.5 mce.c:801-873);
check dsme gives USER state. When SFOS ships mce >= 1.119: `BinaryBrightnessFile=/sys/class/leds/white:kbd_backlight/brightness`.

Layout, least invasive on SFOS = do what hybris proved works, end to end:
1. DT (pro1x-keyboard.patch, SFOS kernel only): `key-super-l`/`key-super-r` `linux,code = <KEY_RIGHTALT>` instead of KEY_FN. Qt's default
   qmap maps RIGHTALT (AltGr), the compositor forwards it, clients see xkb `<RALT>`. Matrix (0,3) already is RIGHTALT; harmless duplicate.
2. One symbols file, no package fork, no rules patch: `/usr/share/X11/xkb/symbols/fxtec_vndr/pro1x` with level3 = the Fn legends
   translated from hosts/mo-qwertz/keyd.conf (keycap authority; do NOT copy Michal's pro1(de): its digit row is stock de ¹²³, the Pro1-X
   caps say shifted symbols):
```
default partial alphanumeric_keys
xkb_symbols "de" {
    include "de(nodeadkeys)"   name[Group1]="Fxtec Pro1X German";
    key <AE01> {[ 1,exclam,exclam ]};  ... <AE10> {[ 0,equal,equal ]};  key <AE11> {[ ssharp,question,question ]};
    key <AD01> {[ q,Q,at ]}; <AD02> {[ w,W,asciicircum ]}; <AD03> {[ e,E,EuroSign ]}; <AD04> {[ r,R,degree ]};
    key <AD07> {[ u,U,braceleft ]}; <AD08> {[ i,I,bracketleft ]}; <AD09> {[ o,O,bracketright ]}; <AD10> {[ p,P,braceright ]};
    key <AD11> {[ udiaeresis,Udiaeresis,asciitilde ]}; <AD12> {[ plus,asterisk,asterisk ]}; <AC08> {[ k,K,ampersand ]};
    key <AC09> {[ l,L,apostrophe ]}; <AC10> {[ odiaeresis,Odiaeresis,backslash ]}; <AC11> {[ adiaeresis,Adiaeresis,numbersign ]};
    key <AB01> {[ y,Y,bar ]}; <AB06> {[ n,N,minus ]}; <AB07> {[ m,M,mu ]};
    include "level3(ralt_switch)"
};
```
3. dconf (vendor.d): `layout='fxtec_vndr/pro1x'`, `variant='de'` (generic rule `* * = pc+%l%(v)`, the form the Pro1 file documents).
   Validate on the host first: `xkbcli compile-keymap --include <dir-with-symbols> --layout fxtec_vndr/pro1x --variant de`.
Alternative, only if the DT must stay KEY_FN (pmOS/keyd parity): build a qmap with `kmap2qmap` adding keycode 464 -> AltGr, pass
`LIPSTICK_OPTIONS=-plugin evdevkeyboard:keymap=/usr/share/qt5/keymaps/pro1x.qmap` in droid-hal-device.conf, and bind `<I472>` to
ISO_Level3_Shift in the symbols file. Untested; more pieces, and Fn then lives on a different evdev device from the letters.
