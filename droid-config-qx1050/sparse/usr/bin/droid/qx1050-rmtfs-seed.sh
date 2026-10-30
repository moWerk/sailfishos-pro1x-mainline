#!/bin/sh
# Seed rmtfs's EFS files from the modem partitions, once. The partitions are only read here:
# from then on the modem's writes go to the files. A missing file is copied again, so deleting
# /var/lib/rmtfs/modem_* returns to the factory EFS at the next boot.
set -e
d=/var/lib/rmtfs
# The EFS holds the IMEI and other modem identity: root only.
mkdir -p -m 700 $d
for p in modemst1:modem_fs1 modemst2:modem_fs2 fsg:modem_fsg fsc:modem_fsc; do
	part=/dev/disk/by-partlabel/${p%%:*} file=$d/${p#*:}
	[ -s $file ] && continue
	dd if=$part of=$file.tmp bs=64k 2>/dev/null
	sync $file.tmp
	mv $file.tmp $file
	echo "seeded $file from $part"
done
