#!/bin/sh
# ExecStartPre of connman: wait for wlan0, then give it the stable address
# (qx1050-wlan-mac.sh). wlan0 comes from ath10k through the modem's WLAN service and can
# appear a minute after boot (later still when the modem restarts once at boot); a unit
# bound to the wlan0 device timed out before that and connman then filed saved networks
# under the random address. Without wlan0 after 120 s connman starts anyway.
i=0
while [ ! -e /sys/class/net/wlan0 ] && [ $i -lt 120 ]; do i=$((i + 1)); sleep 1; done
[ -e /sys/class/net/wlan0 ] || { echo "qx1050-wlan-mac-wait: no wlan0 after 120 s"; exit 0; }
/usr/bin/droid/qx1050-wlan-mac.sh wlan0
echo "qx1050-wlan-mac-wait: wlan0 after $i s, address $(cat /sys/class/net/wlan0/address)"
