#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""While the display is on, keep the little CPUs out of their power collapse state.

Measured on the mainline Pro1-X (2026-11-23, Settings' list, interleaved runs): with the little
cores free to enter "cpu-sleep" (power collapse, 666 us by the device tree) list scrolling runs at
30 fps (median frame 33 ms, 68-78 % of frames late); a 600 us resume-latency constraint on CPU 0-3
only (big cores free) gives 17 ms and 29-32 %. A global QoS of 650 us (little power collapse
blocked) does the same; 700 us and above (system, cluster states blocked) does nothing.
Stock Android holds the same kind of vote while the display is powered (sde 300 us on CPU 0-1,
kgsl 422 us while the GPU is awake).

This follows mce's display state (com.nokia.mce.signal display_status_ind: on, dimmed, off)
and writes /sys/devices/system/cpu/cpuN/power/pm_qos_resume_latency_us for the little cores:
LATENCY_US while on or dimmed, "0" (= no constraint; "n/a" would mean 0 us!) while off and on exit.
"""
import os, signal, subprocess, sys
CPUS = [c for c in os.environ.get("DISPLAYQOS_CPUS", "0,1,2,3").split(",") if c != ""]
LATENCY = os.environ.get("DISPLAYQOS_LATENCY_US", "600")

def set_qos(on):
    for c in CPUS:
        try:
            with open(f"/sys/devices/system/cpu/cpu{c}/power/pm_qos_resume_latency_us", "w") as f:
                f.write(LATENCY if on else "0")
        except OSError as e: print(f"displayqos: cpu{c}: {e}", file=sys.stderr)

def display_state():
    r = subprocess.run(["dbus-send", "--system", "--print-reply", "--dest=com.nokia.mce", "/com/nokia/mce/request",
                        "com.nokia.mce.request.get_display_status"], capture_output=True, text=True)
    return "on" if ('"on"' in r.stdout or '"dimmed"' in r.stdout) else "off"

def main():
    mon = subprocess.Popen(["dbus-monitor", "--system",
                            "type='signal',interface='com.nokia.mce.signal',member='display_status_ind'"],
                           stdout=subprocess.PIPE, text=True)
    def stop(*_):
        set_qos(False); mon.terminate(); sys.exit(0)
    signal.signal(signal.SIGTERM, stop); signal.signal(signal.SIGINT, stop)
    state = display_state(); set_qos(state == "on")
    print(f"displayqos: cpus {CPUS} {LATENCY} us while the display is on; now {state}", flush=True)
    expect = False    # dbus-monitor also prints its own NameAcquired/NameLost signals with strings
    for line in mon.stdout:
        line = line.strip()
        if "member=display_status_ind" in line:
            expect = True
        elif expect and line.startswith('string "'):
            expect = False
            new = "on" if line in ('string "on"', 'string "dimmed"') else "off"
            if new != state:
                state = new; set_qos(state == "on")
    set_qos(False)

if __name__ == "__main__":
    main()
