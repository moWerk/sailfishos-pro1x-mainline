#!/bin/sh
# Move journal files that journald set aside as uncleanly shut down (*.journal~) out of the
# journal directory, where the vacuum would delete them first (their names carry the early
# boot clock, see /etc/systemd/journald.conf.d/90-qx1050-bringup.conf). Keep the last ten.
# Read one with: journalctl --file /var/log/journal-unclean/<name>
keep=/var/log/journal-unclean
mkdir -p "$keep"
chmod 0750 "$keep"
for f in /var/log/journal/*/*.journal~; do
    [ -e "$f" ] || continue
    mv "$f" "$keep/$(date +%Y%m%d-%H%M%S)-$(basename "$f" .journal~).journal"
done
ls -1t "$keep"/*.journal 2>/dev/null | tail -n +11 | while read -r old; do rm -f "$old"; done
exit 0
