#!/bin/sh
# ath10k on mainline finds no MAC address and picks a random one at every boot, so connman
# sees a new network each time. Give wlan0 a stable, locally administered address derived
# from the machine id (same idea as droid-config-victara's victara-wlan-mac).
dev=${1:-wlan0}
h=$(sha256sum /etc/machine-id | cut -c1-10)
mac=$(echo "02${h}" | sed 's/../&:/g; s/:$//')
ip link set dev "$dev" down
ip link set dev "$dev" address "$mac"
