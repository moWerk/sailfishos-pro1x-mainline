#!/bin/sh
# Bring-up workaround: the front sensor (S5K4H7 on i2c 3-002d) sometimes fails its probe at
# boot ("Error reading reg 0x0000: -110", power-up timing in the driver, to be fixed there).
# CAMSS registers its cameras only when every sensor of its async notifier is bound, so one
# failed sensor hides both cameras from libcamera. Retry the bind a few times.
dev=3-002d; drv=/sys/bus/i2c/drivers/s5k4h7
i=0
while [ ! -d $drv ] && [ $i -lt 30 ]; do i=$((i + 1)); sleep 1; done
[ -d $drv ] || { echo "qx1050-camera-probe: no s5k4h7 driver"; exit 0; }
for try in 1 2 3 4 5; do
  [ -e /sys/bus/i2c/devices/$dev/driver ] && { echo "qx1050-camera-probe: s5k4h7 bound (try $try)"; exit 0; }
  echo $dev > $drv/bind 2>/dev/null
  sleep 2
done
[ -e /sys/bus/i2c/devices/$dev/driver ] && echo "qx1050-camera-probe: s5k4h7 bound" || echo "qx1050-camera-probe: s5k4h7 still unbound"
