#!/bin/sh
# Ears-free speaker test (bring-up): record the built-in mic while the speaker plays a tone
# and compare the RMS level with a quiet recording. Prints "quiet=<rms> tone=<rms> PLAYS|SILENT".
# Needs the UCM profile installed (alsaucm), alsa-utils; PulseAudio is suspended around it.
# A kernel known to play must calibrate the threshold once (ratio tone/quiet; default 3).
set -u
ratio=${1:-3}
U="su -l defaultuser -c"
pa() { $U "XDG_RUNTIME_DIR=/run/user/100000 pactl $1 2>/dev/null" 2>/dev/null; }
pa "suspend-source alsa_input.Pro1X.HiFi__Mic__source 1"; pa "suspend-sink alsa_output.Pro1X.HiFi__Speaker__sink 1"
alsaucm -c "F(x)tec Pro1X" set _verb HiFi set _enadev Speaker set _enadev Mic >/dev/null 2>&1
rms() { od -An -v -td2 -w2 "$1" | awk '{s+=$1*$1; n++} END {if (n) printf "%.0f", sqrt(s/n); else print 0}'; }
arecord -q -D hw:0,1 -f S16_LE -r 48000 -c 1 -d 2 /tmp/lb-quiet.wav
q=$(rms /tmp/lb-quiet.wav)
(speaker-test -D hw:0,0 -c2 -t sine -f 440 -l 1 -p 3 >/dev/null 2>&1 &)
sleep 0.5
arecord -q -D hw:0,1 -f S16_LE -r 48000 -c 1 -d 2 /tmp/lb-tone.wav
t=$(rms /tmp/lb-tone.wav)
wait 2>/dev/null
pa "suspend-sink alsa_output.Pro1X.HiFi__Speaker__sink 0"; pa "suspend-source alsa_input.Pro1X.HiFi__Mic__source 0"
verdict=SILENT; [ "$q" -gt 0 ] && [ $((t * 10)) -ge $((q * ratio * 10)) ] && verdict=PLAYS
echo "quiet=$q tone=$t $verdict"
