#!/bin/sh
# Prepare the tree hexagonrpcd serves to the ADSP (Sensor Core), then wait for its
# FastRPC node. Port of the ExecStartPre in fxtec-pro1x-nixos modules/sensors.nix.
set -eu
src=/usr/share/qx1050/hexagonfs
d=/var/lib/hexagonrpcd

mkdir -p "$d/sensors"
# config, sns_reg.conf and the DSP-side modules are static: refresh them every start.
rm -rf "$d/sensors/config" "$d/dsp"
cp -r "$src/sensors/config" "$d/sensors/config"
cp -f "$src/sensors/sns_reg.conf" "$d/sensors/sns_reg.conf"
[ -d "$src/dsp" ] && cp -r "$src/dsp" "$d/dsp"

# The ADSP keeps its registry (parsed config, calibration) in
# sensors/persist/registry/{registry/,sns_reg_version,...} and rewrites it, so it is a
# writable copy, re-seeded only when the packaged registry changes.
reg="$d/sensors/persist/registry"
stamp=$(cd "$src/sensors/registry" && find . -type f | sort | xargs md5sum | md5sum | cut -d' ' -f1)
if [ "$(cat "$d/.seeded-from" 2>/dev/null)" != "$stamp" ] || [ ! -d "$reg/registry" ]; then
    rm -rf "$d/sensors/persist"
    mkdir -p "$reg/registry"
    cp -r "$src/sensors/registry/." "$reg/registry/"
    echo "$stamp" > "$d/.seeded-from"
fi
chmod -R u+rwX "$d"

# Platform properties the ADSP asks for by name; QRD selects the bengal_qrd_* configs,
# which list this phone's sensor chips. Other names fall back to /sys/devices/soc0.
rm -rf "$d/properties"; mkdir -p "$d/properties"
echo QRD > "$d/properties/hw_platform"
ln -sfn /sys/devices/soc0 "$d/socinfo"

# A crashing sensor stack must not restart the ADSP in a loop: that can freeze the
# phone. Disabled, a crash leaves the ADSP down (no audio until reboot).
for rp in /sys/class/remoteproc/*; do
    [ "$(cat "$rp/name" 2>/dev/null)" = adsp ] && echo disabled > "$rp/recovery" || true
done

# The ADSP remoteproc registers late in boot.
i=0
while [ $i -lt 240 ]; do
    [ -e /dev/fastrpc-adsp ] && exit 0
    sleep 1; i=$((i + 1))
done
echo "/dev/fastrpc-adsp did not appear" >&2
exit 1
