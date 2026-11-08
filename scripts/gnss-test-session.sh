#!/bin/sh
# gnss-test-session.sh SECONDS: run the QML PositionSource test client against the native provider and report
# the provider's QMI LOC lines and fix statistics (no coordinates). Run as root.
T=${1:-60}
ENV=$(tr '\0' '\n' < /proc/$(pgrep -f "^/usr/bin/lipstick" | head -1)/environ | grep -E "^(WAYLAND_DISPLAY|XDG_RUNTIME_DIR|DBUS_SESSION_BUS_ADDRESS|QT_QPA_PLATFORM|HOME|LANG)=" | tr '\n' ' ')
since=$(date "+%F %T")
su defaultuser -c "env $ENV sailfish-qml gpstest" >/dev/null 2>&1 &
sleep "$T"
pkill -f "[s]ailfish-qml gpstest"
J=$(journalctl --since "$since" --no-pager -o cat)
echo "$J" | grep -E "QMI LOC|XTRA|xtra|engine state" | grep -vE "position report|satellite" | uniq -c | head -25
P=$(echo "$J" | grep "qml: POS" | grep -v " Invalid ")
echo "fixes: $(echo "$P" | grep -c POS) in $T s"
echo "$P" | grep -q POS && echo "$P" | awk -v t0=$(date -d "$since" +%s) 'NR==1{printf "first fix %.0f s after start\n", $3/1000-t0} NR==1{f=$5; b=$5} $5<b{b=$5} {l=$5} END{printf "accuracy m: first %s, best %s, last %s\n", f, b, l}'
