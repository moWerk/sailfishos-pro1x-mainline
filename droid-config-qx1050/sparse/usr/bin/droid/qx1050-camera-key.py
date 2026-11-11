#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Camera button: hold it while the phone is unlocked to open the camera.

The Pro1-X camera button is one gpio-key with KEY_CAMERA (no focus stage).
lipstick-jolla-home has a ShutterKeyHandler that opens the viewfinder on a
press and hold, but Qt's evdev keymap has no entry for KEY_CAMERA, so the key
never reaches it. This service does the same thing from the input device:
a hold of HOLD_S while mce reports the lock screen unlocked and the display
on calls com.jolla.camera.ui.showViewfinder, the call lipstick makes. Never
from the lock screen, as in Jolla's handler, so a pocket press does nothing.
"""
import os
import select
import struct
import subprocess
import sys
import time

DEVICE = '/dev/input/by-path/platform-gpio-keys-event'
EV_KEY, KEY_CAMERA = 1, 212
HOLD_S = 0.6
EVENT = struct.Struct('llHHi')


def mce(method):
    try:
        out = subprocess.run(['dbus-send', '--system', '--print-reply', '--dest=com.nokia.mce',
                              '/com/nokia/mce/request', 'com.nokia.mce.request.' + method],
                             capture_output=True, text=True, timeout=3).stdout
        return out.split('"')[1] if '"' in out else ''
    except (subprocess.SubprocessError, OSError):
        return ''


def open_camera():
    if mce('get_tklock_mode') != 'unlocked' or mce('get_display_status') != 'on':
        return
    subprocess.run(['dbus-send', '--session', '--type=method_call', '--dest=com.jolla.camera',
                    '/', 'com.jolla.camera.ui.showViewfinder', 'string:'], timeout=5)
    print('camera key: viewfinder requested', flush=True)


def main():
    fd = os.open(DEVICE, os.O_RDONLY)
    pressed_at = None
    fired = False
    while True:
        timeout = None
        if pressed_at is not None and not fired:
            timeout = max(0.0, pressed_at + HOLD_S - time.monotonic())
        ready, _, _ = select.select([fd], [], [], timeout)
        if not ready:
            fired = True
            open_camera()
            continue
        data = os.read(fd, EVENT.size * 16)
        for off in range(0, len(data) - EVENT.size + 1, EVENT.size):
            _, _, etype, code, value = EVENT.unpack_from(data, off)
            if etype != EV_KEY or code != KEY_CAMERA:
                continue
            if value == 1:
                pressed_at, fired = time.monotonic(), False
            elif value == 0:
                pressed_at, fired = None, False


if __name__ == '__main__':
    sys.exit(main())
