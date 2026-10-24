#!/bin/sh
# Workaround (2026-10-24): on some kernel builds the sm4250 sound card comes up silent on the
# first probe (DSP streams, codec and amps powered, nothing audible); re-creating the card
# once makes it play. Which builds are affected depends on boot-time ordering, not on config
# options (bisected: unrelated fragments were "silent", reloading the machine driver on a
# silent kernel plays). Until the race in the kernel is found, re-register the card once
# before the user session (and PulseAudio) starts.
card() { grep -q "Pro1X" /proc/asound/cards 2>/dev/null; }
i=0
while ! card && [ $i -lt 60 ]; do i=$((i + 1)); sleep 1; done
card || { echo "qx1050-audio-reprobe: no sound card after 60 s"; exit 0; }
sleep 2
rmmod snd_soc_sm8250 || exit 0
sleep 1
modprobe snd_soc_sm8250
i=0
while ! card && [ $i -lt 20 ]; do i=$((i + 1)); sleep 1; done
card && echo "qx1050-audio-reprobe: sound card re-registered" || echo "qx1050-audio-reprobe: card did not come back"
