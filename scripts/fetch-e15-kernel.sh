#!/bin/sh
# Fetch a kernel built on the e15 (nix build --out-link /out/<name>) into port/build/<name>:
# Image, sm6115-fxtec-pro1x.dtb and modules.tar (lib/modules/7.1.0).
#   fetch-e15-kernel.sh <name> [host]
set -eu
name=$1; host=${2:-mo@192.168.176.164}
cd "$(dirname "$0")/.."
read -r kdir mdir <<EOF
$(ssh -o BatchMode=yes "$host" "S=\$HOME/sailfish-build; K=\$(readlink \$S/out/$name); K=\$S/nix/\${K#/nix/}; M=\$(grep -oE '/nix/store/[a-z0-9]{32}-linux-aarch64-unknown-linux-gnu-7.1.0-modules' \$S/$name-build.log | sort -u | tail -1); M=\$S/nix/\${M#/nix/}; [ -f \$K/Image ] && [ -d \$M/lib/modules/7.1.0 ] && echo \$K \$M")
EOF
[ -n "${kdir:-}" ] && [ -n "${mdir:-}" ] || { echo "kernel $name not found on $host"; exit 1; }
mkdir -p build/$name
scp -q "$host:$kdir/Image" build/$name/Image
scp -q "$host:$kdir/dtbs/qcom/sm6115-fxtec-pro1x.dtb" build/$name/sm6115-fxtec-pro1x.dtb
ssh -o BatchMode=yes "$host" "tar -C $mdir -chf - lib/modules | gzip -1" | gunzip -c > build/$name/modules.tar
ls -l build/$name
