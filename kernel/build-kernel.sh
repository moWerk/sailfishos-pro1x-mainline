#!/bin/sh
# Build the Pro1-X SailfishOS kernel from a plain kernel tree (no Nix), in a rootless podman
# container with the Debian aarch64 cross compiler.
#   build-kernel.sh <linux tree> <out dir>
# Config: arm64 defconfig + pro1x.fragment + sfos.fragment (this directory), resolved by the
# kernel's own Kconfig; the build FAILS if a fragment line does not end up as requested.
# Output (the layout sfos-kernel-install.sh takes): Image, sm6115-fxtec-pro1x.dtb,
# modules.tar (lib/modules/<release>), config, build.log.
# On the e15, run it inside the user build.slice: BUILD_SLICE=build.slice build-kernel.sh ...
set -eu
src=$(realpath "$1"); out=$(realpath -m "$2"); here=$(dirname "$(realpath "$0")")
img=localhost/qx1050-kernel-builder:trixie-2
mkdir -p "$out"

if ! podman image exists $img; then
    podman build -t $img - <<'EOF'
FROM docker.io/library/debian:trixie
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc libc6-dev gcc-aarch64-linux-gnu make bc bison flex libssl-dev libelf-dev cpio kmod python3 \
    perl rsync ca-certificates && rm -rf /var/lib/apt/lists/*
EOF
fi

{ podman run --rm ${BUILD_SLICE:+--cgroup-parent=$BUILD_SLICE} \
    -v "$src":/src:Z -v "$here":/cfg:ro,Z -v "$out":/out:Z -w /src $img sh -euc '
export ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KBUILD_BUILD_USER=qx1050 KBUILD_BUILD_HOST=sfos
b=/out/obj; mkdir -p $b
make -s O=$b defconfig
grep -hE "^(CONFIG_|# CONFIG_[A-Za-z0-9_]+ is not set)" /cfg/pro1x.fragment /cfg/sfos.fragment >> $b/.config
make -s O=$b olddefconfig 2>&1 | grep -v "override: reassigning" || true
fail=0
grep -hE "^(CONFIG_|# CONFIG_[A-Za-z0-9_]+ is not set)" /cfg/pro1x.fragment /cfg/sfos.fragment | while IFS= read -r l; do
    case "$l" in
    "# CONFIG_"*) o=${l#\# }; o=${o%% *}; grep -q "^$o=" $b/.config && echo "MISMATCH: $o should be unset";;
    *) grep -qxF "$l" $b/.config || echo "MISMATCH: wanted $l, got $(grep "^${l%%=*}=" $b/.config || echo unset)";;
    esac
done > /out/config-check.txt
[ -s /out/config-check.txt ] && { cat /out/config-check.txt; echo "config check FAILED" >&2; exit 1; }
make O=$b -j${JOBS:-12} Image modules qcom/sm6115-fxtec-pro1x.dtb
rm -rf /out/mods; make -s O=$b INSTALL_MOD_PATH=/out/mods INSTALL_MOD_STRIP=1 modules_install
rel=$(cat $b/include/config/kernel.release)
rm -f /out/mods/lib/modules/$rel/build /out/mods/lib/modules/$rel/source
tar -C /out/mods --owner=0 --group=0 -cf /out/modules.tar lib/modules
cp $b/arch/arm64/boot/Image /out/Image
cp $b/arch/arm64/boot/dts/qcom/sm6115-fxtec-pro1x.dtb /out/
cp $b/.config /out/config
echo "kernel release $rel"
' 2>&1 && echo 0 > "$out/rc" || echo $? > "$out/rc"; } | tee "$out/build.log"
exit "$(cat "$out/rc")"
