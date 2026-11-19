#!/bin/sh
# Bring-up workaround: a camera part sometimes fails its probe at boot with -110 (no I2C
# answer; power-up timing in the drivers, to be fixed there). Seen on the second Pro1-X, one
# part per boot at random: the front S5K4H7 (i2c 3-002d), the rear IMX582 (2-0010) and the
# rear lens DW9800 (2-000c). CAMSS registers its cameras only when its async notifier has
# everything bound (the lens is part of the rear sensor's), so one failed part hides both
# cameras from libcamera. Retry the bind of whichever part is unbound, a few times.
# Started by qx1050-camera-probe.timer 40 s after boot, never during the boot itself.
sensors="3-002d:s5k4h7 2-0010:imx582 2-000c:dw9800"
rc=0
for s in $sensors; do
  dev=${s%%:*}; drv=/sys/bus/i2c/drivers/${s#*:}
  i=0
  while [ ! -d $drv ] && [ $i -lt 30 ]; do i=$((i + 1)); sleep 1; done
  [ -d $drv ] || { echo "qx1050-camera-probe: no driver ${s#*:}"; continue; }
  for try in 1 2 3 4 5; do
    [ -e /sys/bus/i2c/devices/$dev/driver ] && { echo "qx1050-camera-probe: ${s#*:} bound (try $try)"; break; }
    echo $dev > $drv/bind 2>/dev/null
    sleep 2
  done
  [ -e /sys/bus/i2c/devices/$dev/driver ] || { echo "qx1050-camera-probe: ${s#*:} still unbound"; rc=1; }
done
exit 0
