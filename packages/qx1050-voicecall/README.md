# qx1050-voicecall: call audio on the mainline port

Ported from fxtec-pro1x-nixos `device/audio` (voicepcm.c unchanged, pro1x-voice-call with
ofono's VoiceCallManager instead of mmcli, gst-launch keep-alive streams instead of
paplay/parec, which SailfishOS lacks). The "Voice Call" UCM verb (`/usr/share/alsa/ucm2/Pro1X/
VoiceCall.conf`) runs `pro1x-voice-call start|stop`; while ofono reports a call the helper holds
the q6voice VoiceMMode1 PCM open (pro1x-voicepcm), which starts the DSP voice session.

The verb is selected by the audio policy: `xpolicy.conf` card profiles. Before 2026-10-27 they
named "HiFi" and "Voice Call", which PulseAudio 17 does not have (its UCM profiles are
"<verb> (<devices>)"), so every call left the card in HiFi: calls connected without audio.

State 2026-10-27, no call made yet: package installed, policy fixed; a manual switch to
"Voice Call (Earpiece, Mic)" runs the verb and the helper, and the switch back to "HiFi (Mic,
Speaker)" stops it. Kernel side effect seen on the switch: wcd937x_codec_enable_ear_pa ->
"Unbalanced enable for IRQ 198" warning (driver bug, harmless so far), plus q6afe "Unknown cmd
0x100f6". Next: a real call, earpiece first, then speaker and headset.

## Call test 2026-10-27 17:00 and fixes (0.1-3, 0.1-4)

- First call: rhythmic "tack" (codec power-cycling, 76 x "wcd937x clk already disabled"); the
  helper looked for NixOS sink names (`platform-sound.*Voice_Call`), started no keep-alive.
  0.1-3 matches `Voice_Call` and logs to `$XDG_RUNTIME_DIR/pro1x-voice-call.log`.
- Second call: slower tack, still no audio either way. Helper log: voice session started on
  hw:0,5 (playback start "Broken pipe", ignored as on NixOS), capture started, but the
  keep-alive restarted again and again on alternating Voice_Call Speaker/Earpiece sinks. The
  policy classifies `gst-launch-1.0` as "player" (route_audio), so each keep-alive moved the
  route to the speaker, the call rerouted to the earpiece, the sink went away: a loop.
  0.1-4 tags the streams `pro1x.voice=keepalive`; xpolicy.conf puts that property in the
  "call" group (verified: policy.group = "call"). Next: a third call.
- One manual profile switch crashed PulseAudio (assertion pa_channels_valid in
  pa_cvolume_set, then pa_idxset_first on restart); not reproduced since. Watch for it.

## Calls 2026-10-28 (0.1-4, 0.1-5)

- The tack loop was the audio policy, not the helper: every UCM route is its own card
  profile, so a profile switch removes the sink, PulseAudio reported all built-in devices
  gone to ohmd ("info 0"), ohmd rerouted, the sink came back ("info 1"), ohmd rerouted again:
  "Voice Call (Earpiece, Mic)" and "Voice Call (Mic, Speaker)" alternated every 0.18 s.
  Found with `dbus-monitor --system` and `pactl subscribe`. Fixed in `xpolicy.conf`
  (disable_notify on the built-in [device] rules); the loop is gone.
- Still silent both ways. The VoiceMMode1 session (VSID 11C05000) opens without errors, the
  voice mixers and the earpiece path are on, the voice PCM runs at 8 kHz, the keep-alive
  streams hold the codec DMA back ends. 0.1-5 opens the session only once the call is active
  (as postmarketOS q6voiced does): no change.
- Open: whether call audio was ever heard on this phone with a mainline kernel; a NixOS
  baseline (kernel 7.1, ModemManager) is being set up. The modem logs a watchdog crash
  ("DOG detects stalled initialization") about 40 s after boot and restarts.
