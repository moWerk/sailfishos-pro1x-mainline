#!/bin/sh
# Copy what ramoops kept from the previous boot (panic log, console, userspace messages) out of
# /sys/fs/pstore before the next crash can overwrite it, into /var/log/pstore/<time>/.
d=/sys/fs/pstore
ls $d/* >/dev/null 2>&1 || exit 0
out=/var/log/pstore/$(date +%Y%m%d-%H%M%S)
mkdir -p "$out" && cp -p $d/* "$out"/ && echo "qx1050-pstore-save: saved $(ls $d | tr '\n' ' ')to $out"
# keep the store for the next crash: remove the entries once they are saved
for f in $d/*; do [ -e "$out/${f##*/}" ] && rm -f "$f"; done
exit 0
