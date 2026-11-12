#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Headphone jack: hand playback to the headphones on plug-in, pause it on unplug.

When a plug or unplug makes PulseAudio switch the card profile (Speaker and
Headphones are separate profiles, they share one PCM) while a stream plays,
the DSP playback port stalls: the stream keeps RUNNING but no audio moves,
until the sink is suspended and resumed (what a pause and play also does).
This service watches the jack's switch events and, once PulseAudio has had
time to switch, suspends every RUNNING ALSA sink for a moment and resumes it.
On unplug every MPRIS player is paused at once, so music does not move to the
speaker and play aloud in a quiet place (the author's choice, 2026-11-11; until
the UCM fix of 2026-11-12 the stall did that by accident).
A workaround: the stall itself is a kernel/DSP issue, see the port README.
"""
import glob
import os
import re
import select
import struct
import subprocess
import sys
import time

EV_SW = 5
JACK_CODES = {2, 4, 6}  # SW_HEADPHONE_INSERT, SW_MICROPHONE_INSERT, SW_LINEOUT_INSERT
SETTLE_S = 1.2
EVENT = struct.Struct('llHHi')


def jack_device():
    for dev in sorted(glob.glob('/dev/input/event*')):
        try:
            with open('/sys/class/input/%s/device/name' % os.path.basename(dev)) as f:
                if 'Headset Jack' in f.read():
                    return dev
        except OSError:
            pass
    return None


def kick():
    out = subprocess.run(['pactl', 'list', 'short', 'sinks'], capture_output=True, text=True).stdout
    for line in out.splitlines():
        f = line.split('\t')
        if len(f) >= 5 and f[1].startswith('alsa_output.') and f[4] == 'RUNNING':
            subprocess.run(['pactl', 'suspend-sink', f[1], '1'])
            time.sleep(0.2)
            subprocess.run(['pactl', 'suspend-sink', f[1], '0'])
            print('jack: restarted %s' % f[1], flush=True)


def pause_players():
    out = subprocess.run(['dbus-send', '--session', '--print-reply', '--dest=org.freedesktop.DBus',
                          '/org/freedesktop/DBus', 'org.freedesktop.DBus.ListNames'],
                         capture_output=True, text=True).stdout
    for name in sorted(set(re.findall(r'"(org\.mpris\.MediaPlayer2\.[^"]+)"', out))):
        subprocess.run(['dbus-send', '--session', '--type=method_call', '--dest=' + name,
                        '/org/mpris/MediaPlayer2', 'org.mpris.MediaPlayer2.Player.Pause'])
        print('jack: paused %s' % name, flush=True)


def main():
    # The sound card is registered again early in boot, which recreates the jack device.
    dev = None
    while dev is None:
        dev = jack_device()
        if dev is None:
            time.sleep(5)
    fd = os.open(dev, os.O_RDONLY)
    due = None
    while True:
        timeout = None if due is None else max(0.0, due - time.monotonic())
        try:
            ready, _, _ = select.select([fd], [], [], timeout)
        except InterruptedError:
            continue
        if not ready:
            due = None
            kick()
            continue
        try:
            data = os.read(fd, EVENT.size * 16)
        except OSError:
            return 1  # device went away (card re-registered): let systemd restart us
        for off in range(0, len(data) - EVENT.size + 1, EVENT.size):
            _, _, etype, code, value = EVENT.unpack_from(data, off)
            if etype != EV_SW or code not in JACK_CODES:
                continue
            if value == 1:
                due = time.monotonic() + SETTLE_S
            elif code in (2, 6):  # headphone or line out removed
                due = None
                pause_players()


if __name__ == '__main__':
    sys.exit(main())
