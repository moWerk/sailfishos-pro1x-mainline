#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Wake the display on any key of the slide keyboard, not only Esc.

mce (1.117) turns Esc into the power key while the display is off or no application expects
input (event-input.c, an N900 convention); the other keys of a slide keyboard do nothing then.
The Pro1-X keyboard is already a wakeup source (the expander's interrupt is armed for wake), so
a key press resumes the SoC and the event arrives; this asks mce to turn the display on for it,
as Esc would. The lock screen stays: this only unblanks.

Reads the "Pro1X keyboard" input device without grabbing it (applications still get every key)
and follows mce's display state (com.nokia.mce.signal display_status_ind). Esc is left to mce.
"""
import os, re, select, signal, struct, subprocess, sys

NAME = os.environ.get("KBDWAKE_DEVICE", "Pro1X keyboard")
EV_KEY, KEY_ESC = 1, 1
EVENT = struct.Struct("llHHi")
MCE = ["dbus-send", "--system", "--print-reply", "--dest=com.nokia.mce", "/com/nokia/mce/request"]


def find_device():
    for block in open("/proc/bus/input/devices").read().split("\n\n"):
        n = re.search(r'N: Name="(.*)"', block)
        h = re.search(r"H: Handlers=.*?(event\d+)", block)
        if n and h and n.group(1) == NAME:
            return "/dev/input/" + h.group(1)
    return None


def display_state():
    r = subprocess.run(MCE + ["com.nokia.mce.request.get_display_status"], capture_output=True, text=True)
    m = re.search(r'string "(\w+)"', r.stdout)
    return m.group(1) if m else "on"


def main():
    path = find_device()
    if not path:
        print(f"kbd-wake: no input device named {NAME!r}", file=sys.stderr)
        return 1
    fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
    mon = subprocess.Popen(["dbus-monitor", "--system",
                            "type='signal',interface='com.nokia.mce.signal',member='display_status_ind'"],
                           stdout=subprocess.PIPE)
    mfd = mon.stdout.fileno()   # read raw: select() does not see lines left in a Python buffer
    signal.signal(signal.SIGTERM, lambda *_: (mon.terminate(), sys.exit(0)))
    state = display_state()
    print(f"kbd-wake: {path} ({NAME}); display {state}", flush=True)
    expect = False   # dbus-monitor also prints its own NameAcquired strings
    buf = b""
    mbuf = b""
    while True:
        r, _, _ = select.select([fd, mfd], [], [])
        if mfd in r:
            chunk = os.read(mfd, 4096)
            if not chunk:
                return 1
            mbuf += chunk
            *lines, mbuf = mbuf.split(b"\n")
            for raw in lines:
                line = raw.decode(errors="replace").strip()
                if "member=display_status_ind" in line:
                    expect = True
                elif expect and line.startswith('string "'):
                    expect = False
                    state = line[8:-1]
        if fd in r:
            try:
                buf += os.read(fd, EVENT.size * 32)
            except BlockingIOError:
                continue
            wake = False
            while len(buf) >= EVENT.size:
                _, _, etype, code, value = EVENT.unpack_from(buf)
                buf = buf[EVENT.size:]
                if etype == EV_KEY and value == 1 and code != KEY_ESC and state == "off":
                    wake = True
            if wake:
                subprocess.run(["dbus-send", "--system", "--type=method_call", "--dest=com.nokia.mce", "/com/nokia/mce/request"]
                               + ["com.nokia.mce.request.req_display_state_on"], check=False)
                state = "on"   # until mce says otherwise


if __name__ == "__main__":
    sys.exit(main())
