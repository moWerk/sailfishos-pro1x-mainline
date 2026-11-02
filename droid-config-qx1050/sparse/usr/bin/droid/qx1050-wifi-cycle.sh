#!/bin/sh
# Workaround (2026-11-02): a modem crash (subsystem restart) puts the phone into 900e crash-dump
# mode about a second after "remote processor modem is now up" when ath10k has run only its
# boot-time power sequence. Nothing is logged; ath10k itself does nothing in that second (traced).
# After one Wi-Fi power cycle (ath10k stop/start, QMI WLAN mode off/on, L8 off/on) every modem
# restart recovered (5/5; without it 3/3 went to 900e). Root cause unknown. So: once Wi-Fi is up,
# switch it off and on through connman. /etc/sysconfig/qx1050-wifi: CYCLE_DISABLE=1 skips it.
CYCLE_DISABLE=0
[ -f /etc/sysconfig/qx1050-wifi ] && . /etc/sysconfig/qx1050-wifi
[ "$CYCLE_DISABLE" = 1 ] && { echo "qx1050-wifi-cycle: disabled"; exit 0; }
up() { [ $(( $(cat /sys/class/net/wlan0/flags 2>/dev/null || echo 0) & 1 )) = 1 ]; }
i=0
while ! up && [ $i -lt 180 ]; do i=$((i + 1)); sleep 1; done
up || { echo "qx1050-wifi-cycle: wlan0 not up after 180 s, Wi-Fi off?"; exit 0; }
# connman is restarted once when wlan0 appears (qx1050-wlan-mac.sh): let that settle, retry
sleep 10
P="dbus-send --system --print-reply --dest=net.connman /net/connman/technology/wifi net.connman.Technology.SetProperty string:Powered variant:boolean"
i=0
until $P:false >/dev/null 2>&1; do
  i=$((i + 1)); [ $i -ge 10 ] && { echo "qx1050-wifi-cycle: connman not answering"; exit 0; }; sleep 3
done
sleep 3
i=0
until $P:true >/dev/null 2>&1; do
  i=$((i + 1)); [ $i -ge 10 ] && { echo "qx1050-wifi-cycle: could not switch Wi-Fi back on"; exit 1; }; sleep 3
done
echo "qx1050-wifi-cycle: Wi-Fi power-cycled"
