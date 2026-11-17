#!/bin/sh
# Copy what ramoops kept from the previous boot (panic log, console, userspace messages) out of
# /sys/fs/pstore before the next crash can overwrite it, into /var/log/pstore/<time>/.
d=/sys/fs/pstore
ls $d/* >/dev/null 2>&1 || exit 0
# the console record of the previous boot is always there; keep the store only after a crash
if ! ls $d/dmesg-* >/dev/null 2>&1; then
  rm -f $d/console-* $d/pmsg-* 2>/dev/null
  exit 0
fi
out=/var/log/pstore/$(date +%Y%m%d-%H%M%S)
mkdir -p "$out" && cp -p $d/* "$out"/ && echo "qx1050-pstore-save: saved $(ls $d | tr '\n' ' ')to $out"
# keep the store for the next crash: remove the entries once they are saved
for f in $d/*; do [ -e "$out/${f##*/}" ] && rm -f "$f"; done
exit 0
