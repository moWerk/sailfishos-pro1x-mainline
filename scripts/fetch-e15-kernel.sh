#!/bin/sh
# Fetch a kernel built on the e15 (nix build --out-link /out/<name>) into port/build/<name>:
# Image, sm6115-fxtec-pro1x.dtb and modules.tar (lib/modules/<version>).
#   fetch-e15-kernel.sh <name> [host]
set -eu
name=$1; host=${2:-mo@192.168.176.164}
cd "$(dirname "$0")/.."
# The modules live in a second output of the kernel derivation; ask nix for all outputs of
# the kernel's deriver (several kernels can share one build log, so the log is no source).
read -r kdir mdir <<EOF
$(ssh -o BatchMode=yes "$host" "S=\$HOME/sailfish-build; K=\$(readlink \$S/out/$name); M=\$(cd \$S && ./nixrun.sh \"nix-store -q --outputs \\\$(nix-store -q --deriver \$K)\" 2>/dev/null | grep -- '-modules\$' | head -1); K=\$S/nix/\${K#/nix/}; M=\$S/nix/\${M#/nix/}; [ -f \$K/Image ] && [ -d \$M/lib/modules ] && echo \$K \$M")
EOF
[ -n "${kdir:-}" ] && [ -n "${mdir:-}" ] || { echo "kernel $name not found on $host"; exit 1; }
mkdir -p build/$name
scp -q "$host:$kdir/Image" build/$name/Image
scp -q "$host:$kdir/dtbs/qcom/sm6115-fxtec-pro1x.dtb" build/$name/sm6115-fxtec-pro1x.dtb
ssh -o BatchMode=yes "$host" "tar -C $mdir -chf - lib/modules | gzip -1" | gunzip -c > build/$name/modules.tar
ls -l build/$name
