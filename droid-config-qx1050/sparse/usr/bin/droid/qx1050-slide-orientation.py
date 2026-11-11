#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Keyboard slide: lock the display to landscape while the keyboard is open.

The slide is SW_KEYPAD_SLIDE on the gpio-keys input device. On the Pro1-X it reads 0
with the keyboard open and 1 with it closed (tested 2026-11-11), the opposite of the
kernel's "set = slide out"; QX1050_SLIDE_OPEN_VALUE overrides it.
When it opens, the current lipstick orientation lock (/lipstick/orientationLock,
the one Settings > Display changes) is saved and replaced by LANDSCAPE; when it
closes, the saved value comes back. The saved value is kept in a file, so a
reboot with the keyboard open still restores the setting on the next close.
Idea after harbour-taskswitcher (Adam Pigg) and tohkbd2 (Kimmo Lindholm), which
do the same for keyboards that report their own slide switch.
"""
import array
import fcntl
import os
import select
import struct
import subprocess
import sys

DEVICE = '/dev/input/by-path/platform-gpio-keys-event'
KEY = '/lipstick/orientationLock'
LANDSCAPE = os.environ.get('QX1050_SLIDE_ORIENTATION', 'landscape')
STATE = os.path.expanduser('~/.local/share/qx1050-slide-orientation/saved')
EV_SW, SW_KEYPAD_SLIDE = 5, 0x0a
OPEN_VALUE = int(os.environ.get('QX1050_SLIDE_OPEN_VALUE', '0'))
EVENT = struct.Struct('llHHi')
EVIOCGSW = (2 << 30) | (8 << 16) | (ord('E') << 8) | 0x1b


def get_lock():
    out = subprocess.run(['dconf', 'read', KEY], capture_output=True, text=True).stdout.strip()
    return out.strip("'") or 'dynamic'


def set_lock(value):
    subprocess.run(['dconf', 'write', KEY, "'%s'" % value], check=False)
    print('slide: orientation lock %s' % value, flush=True)


def slide_open(fd):
    buf = array.array('B', [0] * 8)
    fcntl.ioctl(fd, EVIOCGSW, buf, True)
    return ((buf[SW_KEYPAD_SLIDE // 8] >> (SW_KEYPAD_SLIDE % 8)) & 1) == OPEN_VALUE


def opened():
    current = get_lock()
    if current == LANDSCAPE:
        return  # already landscape (the user's own lock, or ours after a restart)
    os.makedirs(os.path.dirname(STATE), exist_ok=True)
    with open(STATE, 'w') as f:
        f.write(current)
    set_lock(LANDSCAPE)


def closed():
    try:
        with open(STATE) as f:
            saved = f.read().strip() or 'dynamic'
        os.remove(STATE)
    except OSError:
        return  # we did not lock it: leave the user's setting alone
    set_lock(saved)


def main():
    fd = os.open(DEVICE, os.O_RDONLY)
    state = slide_open(fd)
    if state:
        opened()
    elif os.path.exists(STATE):
        closed()
    while True:
        select.select([fd], [], [])
        data = os.read(fd, EVENT.size * 16)
        for off in range(0, len(data) - EVENT.size + 1, EVENT.size):
            _, _, etype, code, value = EVENT.unpack_from(data, off)
            if etype == EV_SW and code == SW_KEYPAD_SLIDE and (value == OPEN_VALUE) != state:
                state = value == OPEN_VALUE
                opened() if state else closed()


if __name__ == '__main__':
    sys.exit(main())
