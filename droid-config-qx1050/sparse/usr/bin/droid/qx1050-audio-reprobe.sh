#!/bin/sh
# Workaround (2026-10-24): on some kernel builds the sm4250 sound card's first instance plays
# silence although the DSP streams and codec and amps are powered. Observed: re-creating the
# card right after boot does not help, but re-creating it after a PCM stream has run on the
# first instance makes the second instance play (same boot, mic loopback oracle: SILENT
# before, PLAYS after). So: open the playback PCM once with a second of digital silence,
# then re-register the card, all before the user session (PulseAudio) starts.
# /etc/sysconfig/qx1050-audio knobs (bring-up): REPROBE_AFTER=<s> (minimum uptime, default 0),
# REPROBE_PRIME=0 (skip the silent stream), REPROBE_DISABLE=1 (do nothing).
REPROBE_AFTER=0; REPROBE_PRIME=1; REPROBE_DISABLE=0
[ -f /etc/sysconfig/qx1050-audio ] && . /etc/sysconfig/qx1050-audio
[ "$REPROBE_DISABLE" = 1 ] && { echo "qx1050-audio-reprobe: disabled"; exit 0; }
card() { grep -q "Pro1X" /proc/asound/cards 2>/dev/null; }
i=0
while ! card && [ $i -lt 60 ]; do i=$((i + 1)); sleep 1; done
card || { echo "qx1050-audio-reprobe: no sound card after 60 s"; exit 0; }
up=$(cut -d. -f1 /proc/uptime)
[ "$up" -lt "$REPROBE_AFTER" ] && sleep $((REPROBE_AFTER - up))
if [ "$REPROBE_PRIME" = 1 ]; then
  # route the front end as PulseAudio's audio-init does, then one second of silence
  amixer -c0 -q cset name='RX_CODEC_DMA_RX_0 Audio Mixer MultiMedia1' 1 2>/dev/null
  alsaucm -c "F(x)tec Pro1X" set _verb HiFi set _enadev Speaker >/dev/null 2>&1
  aplay -q -D hw:0,0 -f S16_LE -r 48000 -c 2 -d 1 /dev/zero 2>/dev/null && echo "qx1050-audio-reprobe: primed"
  alsaucm -c "F(x)tec Pro1X" set _verb HiFi set _disdev Speaker >/dev/null 2>&1
fi
rmmod snd_soc_sm8250 || exit 0
sleep 1
modprobe snd_soc_sm8250
i=0
while ! card && [ $i -lt 20 ]; do i=$((i + 1)); sleep 1; done
card && echo "qx1050-audio-reprobe: sound card re-registered at $(cut -d. -f1 /proc/uptime) s" || echo "qx1050-audio-reprobe: card did not come back"
