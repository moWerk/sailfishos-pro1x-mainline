#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Keep the CPUs out of their deep idle state while the user touches the screen or types.

On the mainline Pro1-X a frame of a scrolling list passes several wake-ups (display and GPU
interrupts, the DRM commit worker, the compositor, the app). Each wake-up from the cores' power
collapse ("cpu-sleep", about 0.65 ms by the device tree) adds latency, and together they push the
page flip past the vblank: lists scroll at 30 fps instead of 60 (measured 2026-11-23: a CPU latency
request of 100 us during scrolling took the median frame interval from 33 to 17 ms).
This holds a PM QoS CPU latency request (/dev/cpu_dma_latency) from the first input event until
HOLD seconds after the last one (a fling keeps scrolling after the finger lifts), and releases it
otherwise, so idle power is unchanged.
"""
import os, select, struct, sys, time, glob, re
LATENCY_US = int(os.environ.get("TOUCHBOOST_LATENCY_US", "100"))
HOLD = float(os.environ.get("TOUCHBOOST_HOLD_S", "2.0"))
# Optional: raise these cpufreq policies' minimum while active ("policy0:1804800,policy4:1056000").
FLOORS = [f.split(":") for f in os.environ.get("TOUCHBOOST_FLOORS", "").split(",") if ":" in f]
CPUFREQ = "/sys/devices/system/cpu/cpufreq"

def set_floors(active):
    for pol, khz in FLOORS:
        try:
            open(f"{CPUFREQ}/{pol}/scaling_min_freq", "w").write(khz if active else open(f"{CPUFREQ}/{pol}/cpuinfo_min_freq").read().strip())
        except OSError as e: print(f"touchboost: {pol}: {e}", file=sys.stderr)
EV_ABS, EV_KEY = 3, 1

def input_devices():
    """Touchscreens (ABS_MT_POSITION_X) and keyboards, by /proc/bus/input/devices."""
    out = []
    for d in open("/proc/bus/input/devices").read().split("\n\n"):
        h = re.search(r"H: Handlers=.*?(event\d+)", d); n = re.search(r'N: Name="(.*)"', d)
        a = re.search(r"B: ABS=([0-9a-f ]+)", d); k = re.search(r"B: KEY=([0-9a-f ]+)", d)
        if not h: continue
        touch = a and int(a.group(1).replace(" ", ""), 16) >> 0x35 & 1
        kbd = n and "keyboard" in n.group(1).lower()
        if touch or kbd: out.append("/dev/input/" + h.group(1))
    return out

def main():
    fds = []
    for p in input_devices():
        try: fds.append(os.open(p, os.O_RDONLY | os.O_NONBLOCK))
        except OSError as e: print(f"touchboost: {p}: {e}", file=sys.stderr)
    if not fds: sys.exit("touchboost: no input devices")
    print(f"touchboost: watching {len(fds)} devices, {LATENCY_US} us while active, hold {HOLD} s, floors {FLOORS}", flush=True)
    qos, last = None, 0.0
    while True:
        timeout = max(0.0, last + HOLD - time.monotonic()) if qos is not None else None
        r, _, _ = select.select(fds, [], [], timeout)
        if r:
            for fd in r:
                try: os.read(fd, 24 * 64)
                except OSError: pass
            last = time.monotonic()
            if qos is None:
                qos = os.open("/dev/cpu_dma_latency", os.O_WRONLY)
                os.write(qos, struct.pack("i", LATENCY_US))
                set_floors(True)
        elif qos is not None and time.monotonic() >= last + HOLD:
            os.close(qos); qos = None   # closing the file drops the request
            set_floors(False)

if __name__ == "__main__":
    main()
