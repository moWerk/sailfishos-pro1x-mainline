#!/bin/sh
# Copy lvm, e2fsck and the libraries they load from a running SailfishOS into a local tree,
# for LVM_SYSROOT= of scripts/build-initramfs.sh. Only reads on the phone.
#   fetch-lvm-sysroot.sh <ssh destination, e.g. root@192.168.2.15> <dir>
# SSH="ssh -o ..." to pass ssh options.
set -eu
[ $# -eq 2 ] || { echo "usage: $0 <user@phone> <dir>"; exit 1; }
host=$1; dir=$2
mkdir -p "$dir"
# ldd resolves the whole closure on the phone; tar -h stores the files behind symlinks.
${SSH:-ssh} "$host" 'cd / && b="usr/sbin/lvm $(readlink -f /sbin/e2fsck | cut -c2-)" && tar -chf - $b lib/ld-linux-aarch64.so.1 $(for x in $b; do ldd /$x; done | sed -n "s|.*=> /\([^ ]*\) .*|\1|p" | sort -u)' \
  | tar -xf - -C "$dir"
find "$dir" -type f | sed "s|^$dir/||" | sort
