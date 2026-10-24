#!/bin/sh
# Workaround (2026-10-24): on some kernel builds the sm4250 sound card comes up silent on the
# first probe (DSP streams, codec and amps powered, nothing audible); re-creating the card
# later makes it play. Which builds are affected depends on boot-time ordering, not on config
# options (bisected: unrelated fragments were "silent", reloading the machine driver on a
# silent kernel plays). A re-probe at 12 s after boot was still silent, one minutes later
# played; clock controllers reach sync_state around 20-23 s. Until the race is found in the
# kernel, re-register the card once, not before REPROBE_AFTER seconds of uptime.
# /etc/sysconfig/qx1050-audio may set REPROBE_AFTER (bring-up knob for the timing search).
REPROBE_AFTER=30
[ -f /etc/sysconfig/qx1050-audio ] && . /etc/sysconfig/qx1050-audio
card() { grep -q "Pro1X" /proc/asound/cards 2>/dev/null; }
i=0
while ! card && [ $i -lt 60 ]; do i=$((i + 1)); sleep 1; done
card || { echo "qx1050-audio-reprobe: no sound card after 60 s"; exit 0; }
up=$(cut -d. -f1 /proc/uptime)
[ "$up" -lt "$REPROBE_AFTER" ] && sleep $((REPROBE_AFTER - up))
rmmod snd_soc_sm8250 || exit 0
sleep 1
modprobe snd_soc_sm8250
i=0
while ! card && [ $i -lt 20 ]; do i=$((i + 1)); sleep 1; done
card && echo "qx1050-audio-reprobe: sound card re-registered at $(cut -d. -f1 /proc/uptime) s" || echo "qx1050-audio-reprobe: card did not come back"
