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
