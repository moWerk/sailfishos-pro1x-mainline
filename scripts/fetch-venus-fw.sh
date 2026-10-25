#!/bin/sh
# Venus video firmware for the SailfishOS kernel (sfos-venus.patch): linux-firmware's
# qcom/venus-6.0/venus.mbn, VIDEO.VE.6.0-00055 (linux-firmware commit 8ecf764788f8).
# The Fxtec vendor image (qcom/sm6115/Fxtec/QX1050/venus.mbn, 6.0-00049) is older than
# the 6.0.55 the venus driver requires on QCM2290/SM6115 and fails the probe with -EINVAL.
# Licence: redistributable, LICENSE.qcom (linux-firmware WHENCE).
# Run on the host before stage 2; writes into build/fw.
set -e
cd "$(dirname "$0")/.."
base=https://gitlab.com/kernel-firmware/linux-firmware/-/raw/8ecf764788f8
d=build/fw/lib/firmware/qcom/venus-6.0
mkdir -p $d
curl -sSfL -o $d/venus.mbn $base/qcom/venus-6.0/venus.mbn
curl -sSfL -o build/fw/lib/firmware/LICENSE.qcom $base/LICENSES/LICENSE.qcom 2>/dev/null \
    || curl -sSfL -o build/fw/lib/firmware/LICENSE.qcom $base/LICENSE.qcom
echo "602d18992dbc11c7141472d4c68778d49277491ca7452ebf6115be0e6119572a  $d/venus.mbn" | sha256sum -c -
