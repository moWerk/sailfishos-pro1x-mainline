#!/bin/sh
# Bring-up black box: every 20 s, write what the system is doing to /var/log/blackbox/,
# readable from the NixOS recovery boot when SailfishOS is not reachable.
d=/var/log/blackbox
mkdir -p $d
n=0
while true; do
  {
    echo "=== $(cat /proc/uptime)"
    ip -br addr
    systemctl list-jobs --no-pager
    systemctl --failed --no-pager --no-legend
    systemctl status --no-pager -n 15 user@100000.service start-user-session.service autologin@100000.service 2>&1
    ls /run/user/100000 2>&1 | head
    ls -la /dev/dri /dev/input 2>&1
  } > $d/now.txt 2>&1
  [ $n -lt 30 ] && cp $d/now.txt $d/t$(printf %02d $n).txt
  journalctl -b --no-pager > $d/journal.txt 2>&1
  dmesg > $d/dmesg.txt 2>&1
  sync
  n=$((n + 1))
  sleep 20
done
