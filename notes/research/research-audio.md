# Audio on SFOS 5.1.0.11 / mainline 7.1 Pro1-X (qx1050): research, 2026-10-23
Labels: C confirmed (file read / repo metadata fetched), I inferred. Read-only research, no device access.
Flake = /mnt/storage/sailfish/pro1x-mainline/fxtec-pro1x-nixos; rootfs = /mnt/storage/sailfish/pro1x-mainline/port/rootfs.

## 1. How the NixOS flake does audio (C)
- Stack: PipeWire + WirePlumber (+ pulse shim), NOT PulseAudio. modules/audio.nix: `services.pipewire.{alsa,pulse,wireplumber}.enable`,
  `ALSA_CONFIG_UCM2` forced to a symlinkJoin of flake UCM + stock alsa-ucm-conf; `wireplumber.serviceConfig.ExecStartPre = pro1x-audio-init`.
- UCM2 (device/audio/ucm2/): `Pro1X/Pro1X.conf` (verbs HiFi + "Voice Call", BootSequence), `Pro1X/HiFi.conf`, `Pro1X/VoiceCall.conf`;
  lookup symlinks `conf.d/sm4250/"F(x)tec Pro1X.conf"` and `conf.d/sm4250/Pro1X.conf` -> ../../Pro1X/Pro1X.conf (relative symlinks, copy with cp -a).
  Devices: Speaker (prio 200, hw:X,0), Headphones (100, hw:X,0), Earpiece (50 HiFi / 200 VoiceCall, hw:X,0), Mic (DMIC3 via VA macro, hw:X,1).
  No JackControl/JackDev, no "Headset" capture device.
- Kernel (device/kernel/patches/pro1x-audio.patch): card `fxtec,pro1x-sndcard`, model "F(x)tec Pro1X", sm8250.c match data "sm4250"
  (= UCM driver dir), codec `qcom,wcd9370-codec` (wcd937x driver, CONFIG_SND_SOC_WCD937X=m), rx/tx/va LPASS macros, two Awinic AW87339
  class-K speaker amps (new driver sound/soc/codecs/aw87339.c, i2c 0x59/0x5a, enable gpios tlmm 85/98) behind HPHL/HPHR, pin-switches
  "Spk0 OUT"/"Spk1 OUT". FE: MultiMedia1 playback = pcm0, MultiMedia2 capture = pcm1, MultiMedia3; BE: RX_CODEC_DMA_RX_0, VA_CODEC_DMA_TX_0,
  TX_CODEC_DMA_TX_3. No MBHC/jack properties in the DTS (only a comment); README: "headset jack detection untested".
- pro1x-q6voice.patch: adds q6mvm/q6cvs/q6cvp/q6voicedai (VOICEMMODE1 DAI link) = postmarketOS-style q6voice; CONFIG_SND_SOC_QDSP6_Q6VOICE(_DAI)=m.
- Call audio: VoiceCall verb EnableSequence routes `RX_CODEC_DMA_RX_0 Voice Mixer VoiceMMode1` + `VoiceMMode1 Capture Mixer VA_CODEC_DMA_TX_0`
  and `exec pro1x-voice-call start`; that script keeps paplay/parec keep-alive streams on the Voice_Call sink/source and starts
  `pro1x-voicepcm` (voicepcm.c, opens the VoiceMMode1 PCM) when **mmcli** (ModemManager) reports a call. Comment in script: starting it
  before a call does not work. SFOS has oFono, not ModemManager -> script falls into its `always=1` branch, which it says fails. Needs rework.
- pro1x-audio-init (before the audio server): waits for the card, sets `RX_CODEC_DMA_RX_0 Audio Mixer MultiMedia1`=1 and
  `MultiMedia2 Mixer VA_CODEC_DMA_TX_0`=1 (FE PCM open fails EINVAL "no backend DAIs enabled" without a routed BE; the server probes
  every PCM), HPHL/HPHR Volume 12, all *_COMP switches 0 (compander makes right louder than left). Needs `amixer` (alsa-utils).
- wireplumber.conf.d/52-pro1x-s16.conf: S24_LE comes out ~40 dB too quiet; force S16LE 48000. Same applies to PA (daemon.conf).
- Pops and clicks: **nothing** in the flake (grep pop/click over device/, modules/, packages/, README: 0 hits). The "twice on boot, once on USB
  plug" line is mo's own observation in reports/pro1x-mainline-port-log.md, made on NixOS. So it is kernel/hardware level, no PA tweak touches it.
  Candidates (I): (a) aw87339_i2c_probe requests enable as GPIOD_OUT_HIGH, reads chip ID, then drives low: a ~2 ms amp-on pulse while the
  codec is unconfigured, once per amp = two boot pops? Test: GPIOD_OUT_LOW and raise only around the ID read. (b) DAPM: the amp is a PGA_E with
  PRE_PMU on / POST_PMD off and use_pmdown_time=1; PGA widgets power up before and down after the HPH stage, so the amp is live while HPH ramps,
  a pop per stream start/stop; `module-suspend-on-idle timeout=1` (pinephone .pa) would cycle it per notification sound -> use a longer timeout.
  (c) USB plug: no candidate at all; experiment: watch tlmm 85/98 (gpio debugfs) and /sys/kernel/debug/asoc/*/dapm across a plug event.

## 2. How mainline SFOS ports do audio (C from ref/droid-config-pinephone, the only truly native reference here)
- ref/droid-config-victara is still hybris for audio (sparse/etc/pulse/arm_droid_card_custom.pa: module-droid-card). Not usable as a model.
- PinePhone pattern (patterns-sailfish-device-adaptation-pinephone.inc): pulseaudio-module-keepalive, alsa-utils, alsa-ucm-conf,
  droid-config-pinephone-pulseaudio-settings, -policy-settings, qt5-qtmultimedia-plugin-mediaservice-gstmediacapture (recording).
- Template (piggz/droid-hal-configs, branch native, droid-configs.inc; spec sets `%define native_build 1`):
  `pulseaudio-settings` Requires pulseaudio, pulseaudio-modules-nemo-{parameters,stream-restore,mainvolume}, pulseaudio-module-keepalive
  (instead of -droid), pulseaudio-policy-enforcement; `policy-settings` Requires ohm, ohm-plugins-misc, ohm-plugins-dbus, ohm-plugin-
  {telephony,signaling,media,accessories,resolver,ruleengine,profile,route}, policy-settings-common, Obsoletes ohm-configs-default
  ("should not be installed ever"). Template sparse/etc/pulse ships x-maemo-{route,match,stream-restore,sink-volume}.table and base xpolicy.conf(.d).
- Pinephone files: `/etc/sysconfig/pulseaudio` = `CONFIG="-n --file=/etc/pulse/arm_native_default.pa"` (user pulseaudio.service reads it);
  `arm_native_default.pa`: module-keepalive, **module-alsa-card device_id=PinePhone** (UCM used automatically, PA ports = UCM devices),
  module-meego-parameters (use_voice=false), module-meego-mainvolume, module-stream-restore-nemo (route tables), module-match,
  null sink/source, module-bluetooth-discover headset=droid, **module-policy-enforcement**, module-role-ducking, module-suspend-on-idle timeout=1,
  set-default-sink sink.null (policy moves streams). `daemon.conf.d/40-pinephone.conf`: default-sample-rate 48000, realtime-scheduling no.
  `xpolicy.conf` (full replacement): `card = alsa_card.PinePhone`, `card_name = PinePhone`, `card_profile_default = HiFi`,
  `card_profile_voicecall = "Voice Call"`, `sink_speaker = "[Out] Speaker"`, `sink_headset = "[Out] Headphones"`, `sink_earpiece = "[Out] Earpiece"`,
  `source_builtin_mic = "[In] Mic"`, `source_headset = "[In] Headset"`; devices matched by `alsa.card_name@equals:$card_name`;
  `xpolicy.conf.d/earpiece.conf`, `bluez5.conf`, `bluez5_earpiece.conf` map ohm route types (ihf, headset, earpiece, *forcall) to card
  profile + port. `unmute-sound-card.sh`: amixer unmute + pacmd set-default-sink (their audio-init equivalent).
- Routing chain (C from sources): ohm-plugin-accessories (plugins/accessories/wired.c) reads SW_HEADPHONE_INSERT / SW_MICROPHONE_INSERT /
  SW_LINEOUT_INSERT from an input device whose name matches "Headset Jack", " Jack" or "ACCDET" (param jack-device overrides) -> ohm route
  plugin picks a route type -> module-policy-enforcement sets card profile/port per xpolicy [card]/[device]. ohm-plugin-telephony sets
  context `call` from oFono -> [context-rule] -> x-nemo.voicecall.status -> *forcall types -> profile "Voice Call". That profile switch is what
  would run the UCM VoiceCall verb (and its exec). (I) sm8250.c creates a "Headset Jack" ASoC jack only when the codec has MBHC configured.

## 3. SFOS 5.1.0.11 repos (C, primary.xml fetched 2026-10-23, saved in scratchpad/repo/)
- jolla: pulseaudio 17.0+git1-1.12.1, pulseaudio-modules-nemo 14.2.32 (common, parameters, stream-restore, mainvolume, voice, sidetone, record,
  music), pulseaudio-policy-enforcement 14.2.49, pulseaudio-module-keepalive 1.1.1, ohm 1.3.2, ohm-plugins-misc 1.10.1 (+ all plugins above),
  policy-settings-common 0.13.0, alsa-lib 1.2.13+git1, alsa-utils 1.2.13+git1, ngfd 1.4.13. No pipewire/wireplumber in jolla, adaptation-common
  or native-common (0 hits). native-common: alsa-ucm-conf 1.2.9.1-1.5.3.bso (ships Qualcomm/ sdm845, sm8250, sc7180...; no sm4250).
- Rootfs now: pulseaudio 17 + nemo common/mainvolume/stream-restore + ohm/ohm-plugins-misc/route/mdm + **ohm-configs-default** (template
  says never install it) installed; `/etc/sysconfig/pulseaudio` empty (PA would start stock default.pa with module-udev-detect);
  module-policy-enforcement.so, module-keepalive.so, module-meego-parameters.so, alsa-utils, alsa-ucm-conf, x-maemo tables all missing;
  user pulseaudio.service exists, enabled in user-session.target.wants. PA's module-alsa-card is UCM-capable (pa_alsa_ucm_* symbols, C).

## 4. Steps to get speaker + headphones + mic on SFOS with PulseAudio
Stage A, plain PA + UCM (verify hardware path first):
1. zypper in alsa-utils alsa-ucm-conf (native-common) pulseaudio-module-keepalive pulseaudio-modules-nemo-parameters pulseaudio-policy-enforcement.
2. cp -a flake device/audio/ucm2/Pro1X and conf.d/sm4250 into /usr/share/alsa/ucm2/ (keep symlinks). Check /proc/asound/cards for the
   card id/name on the device (I: name "F(x)tec Pro1X", id likely FxtecPro1X); `alsaucm -c <name> listall` must list HiFi/Voice Call.
3. Install pro1x-audio-init as /usr/bin/droid/qx1050-audio-init; drop-in /etc/systemd/user/pulseaudio.service.d/10-qx1050.conf with an extra
   ExecStartPre= (unit already has one; append, do not replace).
4. /etc/pulse/daemon.conf.d/40-qx1050.conf: default-sample-rate=48000, default-sample-format=s16le (flake S24 finding), realtime-scheduling=no.
5. Stage A uses a MINIMAL /etc/pulse/arm_native_default.pa under `.nofail`: module-alsa-card device_id=<id from step 2>,
   module-native-protocol-unix, module-always-sink, module-suspend-on-idle timeout=5..10 (not pinephone's 1 s, until the amp pop is understood).
   Do not use pinephone's full .pa yet: it opens with `.fail`, and module-stream-restore-nemo/module-match need the x-maemo tables (step 7) and
   module-meego-parameters/policy-enforcement .so files that are not in the rootfs; one failing load-module aborts the daemon. (Its
   `module-bluetooth-discover headset=droid` argument is a Jolla patch, unverified in PA 17.0+git1.) /etc/sysconfig/pulseaudio
   `CONFIG="-n --file=/etc/pulse/arm_native_default.pa"`. Note: audio-init waits up to 90 s for the card while pulseaudio.service is
   Type=notify with the default start timeout; cut the loop or add TimeoutStartSec in the drop-in.
   Then: `pactl list cards` -> profile HiFi, ports [Out] Speaker/Headphones/Earpiece, [In] Mic;
   `pactl set-sink-port ... "[Out] Headphones"`, paplay / parec. Headphones stay a MANUAL port switch: no MBHC in the DTS, no jack input device,
   so ohm accessories has nothing to read (I). Fix later in the kernel (wcd937x MBHC DT props), not in PA.
Stage B, SFOS policy: 6. zypper in policy-settings-common ohm-plugins-dbus ohm-plugin-{telephony,signaling,media,accessories,resolver,
   ruleengine,profile}; remove ohm-configs-default. 7. Fill port/droid-config-qx1050/droid-configs-device (piggz native branch) so the
   template's x-maemo tables ship, set `%define native_build 1`, add the pinephone pattern lines; now switch to pinephone's full
   arm_native_default.pa (device_id, suspend timeout as in step 5). 8. xpolicy.conf = pinephone's with `card = alsa_card.<device_id>` and
   `card_name = "F(x)tec Pro1X"` QUOTED (alsa.card_name is the card's name with space and parentheses; pinephone's single-token values
   must not be copied as a pattern or every [device] rule silently mis-matches), port names from step 5; `source_headset = "[In] Mic"`
   (flake UCM has no Headset capture device); keep earpiece.conf/bluez5*.conf.
Stage C, calls (out of scope now): q6voice PCM session must be driven from oFono call state, not mmcli; see section 1.
