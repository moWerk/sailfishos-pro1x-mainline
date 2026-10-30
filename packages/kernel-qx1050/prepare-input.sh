#!/bin/sh
# Collect the build inputs of kernel-qx1050 into input/ (not in git):
#   prepare-input.sh <sfos-boot.img> <modules.tar>
# sfos-boot.img from scripts/build-bootimg.sh, modules.tar as the kernel build writes
# it (lib/modules/7.3.0-qx1050/...). Then build with sfos-mb2-build.sh as usual.
set -eu
[ $# -eq 2 ] || { echo "usage: $0 sfos-boot.img modules.tar"; exit 1; }
cd "$(dirname "$0")"
mkdir -p input
cp "$1" input/sfos-boot.img
cp "$2" input/modules.tar
tar -tf input/modules.tar | grep -q '^\(\./\)\{0,1\}lib/modules/7.3.0-qx1050/' \
  || { echo "modules.tar has no lib/modules/7.3.0-qx1050/"; exit 1; }
ls -l input
