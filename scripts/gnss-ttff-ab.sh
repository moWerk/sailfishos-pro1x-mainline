#!/bin/sh
# gnss-ttff-ab.sh: time to first fix from a cold start, A without and B with XTRA, through the native
# geoclue provider and a QML PositionSource client. Runs on the phone by itself (root), logs to
# /root/gps-ab.log, prints no coordinates. Each run: delete all assistance data, start the
# client, wait for the first valid fix (12 min at most), keep it 60 s, stop. Below 30 % battery
# everything stops.
L=/root/gps-ab.log
INI=/etc/gps_xtra.ini
log() { echo "$(date -u +%T) $*" >> $L; }
batt() { cat /sys/class/power_supply/battery/capacity; }
client_stop() {
  p=$(pgrep -f "^sailfish-qml gpstest"); [ -n "$p" ] && kill $p
  sleep 40    # the provider stops the session and exits 30 s after its last client
}
trap 'client_stop; [ -e $INI.off ] && mv $INI.off $INI; log "aborted"; exit 1' TERM INT
ENV="$(tr '\0' '\n' < /proc/$(pgrep -f "^/usr/bin/lipstick" | head -1)/environ | grep -E "^(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|DBUS_SESSION_BUS_ADDRESS|HOME|LANG)=" | tr '\n' ' ') QT_QPA_PLATFORM=wayland"

run() {
  name=$1
  log "== run $name, battery $(batt) %"
  qmicli -d qrtr://0 --loc-delete-assistance-data >/dev/null 2>&1 && log "assistance data deleted"
  sleep 5
  since=$(date "+%F %T"); t0=$(date +%s)
  su defaultuser -c "env $ENV sailfish-qml gpstest" >/dev/null 2>&1 &
  fix=
  while [ $(( $(date +%s) - t0 )) -lt 720 ]; do
    [ "$(batt)" -lt 30 ] && { log "battery below 30 %, stopping"; client_stop; return 1; }
    p=$(journalctl --since "$since" --no-pager -o cat | grep "qml: POS" | grep -v " Invalid " | head -1)
    if [ -n "$p" ]; then
      fix=$(( $(echo "$p" | awk '{print int($3/1000)}') - t0 ))
      log "run $name: first fix after $fix s, accuracy $(echo "$p" | awk '{print $(NF-1)}') m"
      break
    fi
    sleep 2
  done
  [ -z "$fix" ] && log "run $name: no fix in 720 s"
  [ -n "$fix" ] && sleep 60
  J=$(journalctl --since "$since" --no-pager -o cat)
  P=$(echo "$J" | grep "qml: POS" | grep -v " Invalid ")
  log "run $name: $(echo "$P" | grep -c POS) fixes, last accuracy $(echo "$P" | tail -1 | awk '{print $(NF-1)}') m"
  echo "$J" | grep -E "QMI LOC|XTRA|xtra" | grep -vE "position report|satellite" | sed "s/^/   $name: /" | uniq >> $L
  client_stop
}

log "start; kernel $(uname -v | cut -d' ' -f1)"
mv $INI $INI.off && log "XTRA servers hidden"
run A
mv $INI.off $INI && log "XTRA servers back"
run B
log "done, battery $(batt) %"
