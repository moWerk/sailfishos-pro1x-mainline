#!/usr/bin/env bash
# Feel the AW86224 vibration motor at different settings (after the Guix port's
# tools/haptics-tune.sh). As root on the phone, with the screen on and the phone in hand:
#   haptics-tune.sh            sweep from stock to strongest, one buzz each, then restore
#   haptics-tune.sh GAIN DRV2  one buzz and keep it, e.g.  7 100
# GAIN = output gain code 0-7 (x1 x2 x4 x5 x8 x10 x20 x40; stock 5)
# DRV2 = sustained drive level 0-127 (stock 59)
# The driver reads both parameters at every effect, so no module reload is needed. Buzzes are short
# and spaced out because overdriving warms the motor. Make a setting permanent in
# droid-config-qx1050/sparse/etc/modprobe.d/aw86224.conf (now gain 7 / drive 100).
set -u
[ "$(id -u)" = 0 ] || {
  echo "run as root"
  exit 1
}
par=/sys/module/aw86224/parameters
[ -d "$par" ] || {
  echo "aw86224 module not loaded (modprobe aw86224)"
  exit 1
}
dev=""
for d in /sys/class/input/event*; do
  [ "$(cat "$d/device/name" 2>/dev/null)" = aw86224-haptics ] && dev=/dev/input/$(basename "$d")
done
[ -n "$dev" ] || {
  echo "no aw86224-haptics input device"
  exit 1
}

# One 400 ms FF_RUMBLE at full strength. struct ff_effect and EVIOCSFF as on aarch64 (48 bytes).
buzz() {
  python3 - "$dev" <<'PY'
import os, struct, sys, time, fcntl
fd = os.open(sys.argv[1], os.O_RDWR)
buf = bytearray(48)
struct.pack_into("<HhH", buf, 0, 0x50, -1, 0)     # FF_RUMBLE, new effect id, direction
struct.pack_into("<HH", buf, 10, 400, 0)          # replay length, delay
struct.pack_into("<HH", buf, 16, 0xffff, 0xffff)  # strong, weak magnitude
fcntl.ioctl(fd, 0x40304580, buf)                  # EVIOCSFF
eid = struct.unpack_from("h", buf, 2)[0]
os.write(fd, struct.pack("llHHi", 0, 0, 0x15, eid, 1))  # EV_FF play
time.sleep(0.6)
PY
}

try() { # gain drv2
  echo "$1" >$par/d2s_gain
  echo "$2" >$par/drv2_lvl
  echo "gain=$1 drv2=$2 ..."
  buzz
  sleep 2
}

old_g=$(cat $par/d2s_gain)
old_d=$(cat $par/drv2_lvl)
if [ $# -ge 2 ]; then
  try "$1" "$2"
  echo "kept gain=$1 drv2=$2 until reboot (was $old_g/$old_d)"
else
  for combo in "5 59" "6 59" "6 80" "7 59" "7 80" "7 100" "7 127"; do
    try $combo
  done
  echo "restoring gain=$old_g drv2=$old_d"
  echo "$old_g" >$par/d2s_gain
  echo "$old_d" >$par/drv2_lvl
fi
