#!/bin/sh
# Run as root on the phone. Per boot from the given journal offset on: modem crashes, qrtr
# announcement failures (notes/modem-dog-crash.md) and ath10k freq-0 warnings.
# Usage: modem-bootcount.sh [first-offset, default -20]
first=${1:--20}
journalctl --list-boots --no-pager | awk -v f=$first '$1>=f {print $1}' | while read b; do
	c=$(journalctl -k -b $b --no-pager -o cat 2>/dev/null | grep -ci "crash detected\|fatal error")
	q=$(journalctl -k -b $b --no-pager -o cat 2>/dev/null | grep -c "failed to announce")
	w=$(journalctl -k -b $b --no-pager -o cat 2>/dev/null | grep -c "invalid frequency")
	echo "boot $b  crash=$c announce_fail=$q freq0=$w"
done
