#!/bin/sh
# Modem preparation before ofono (qrtrqmi driver, upstream ofono >= 2.9):
# 1. The modem firmware (MPSS.HA.1.1) does not keep the UIM primary GW provisioning session;
#    without it the SIM stays "detected" and nothing registers. Activate it for the first
#    USIM application on slot 1, every boot, idempotently (postmarketOS msm-modem-uim-selection
#    does the same). Needs qmicli (libqmi-utils).
# 2. ofono's udevng builds the qrtrsoc modem only when rmnet_ipa0 and three rmnet_dataN links
#    exist (plugins/udevng.c setup_qrtrsoc: premux_count >= 3, mux_id = N + 1). The IPA
#    offloads MAPv4 checksums (device/feature/*_offload), so the links need the matching rmnet
#    flags as ModemManager sets them; iproute2 has no rmnet flags option, qmicli does.
# 3. The modem sometimes crashes once during boot ("DOG detects stalled initialization",
#    notes/modem-dog-crash.md) and announces UIM only after its recovery, 90-100 s in. So
#    the wait is long, and afterwards a watch loop prepares the modem again and restarts
#    ofono whenever UIM disappears and comes back (a modem restart drops the session and
#    the links; an ofono started on an unprepared modem hangs, and lipstick with it).
log() { echo "qx1050-modem-prep: $*"; }
uim() { qrtr-lookup 2>/dev/null | awk '$1 == 11 { f = 1 } END { exit !f }'; }

prep() {
  i=0
  while ! qmicli -d qrtr://0 --uim-get-card-status 2>/dev/null | grep -q "Card state: 'present'" && [ $i -lt 20 ]; do i=$((i + 1)); sleep 1; done
  status=$(qmicli -d qrtr://0 --uim-get-card-status 2>/dev/null)
  if echo "$status" | grep -q "Primary GW:.*session doesn't exist"; then
    aid=$(echo "$status" | awk '/Application type:.*usim/ { f = 1 } f && /Application ID:/ { getline; gsub(/[^0-9A-Fa-f]/, ""); print; exit }')
    if [ -n "$aid" ]; then
      qmicli -d qrtr://0 --uim-change-provisioning-session="session-type=primary-gw-provisioning,activate=yes,slot=1,aid=$aid" >/dev/null 2>&1 \
        && log "primary GW provisioning session activated" || log "provisioning activation failed"
    else
      log "no USIM application found"
    fi
  else
    log "primary GW provisioning session present"
  fi
  i=0
  while [ ! -e /sys/class/net/rmnet_ipa0 ] && [ $i -lt 120 ]; do i=$((i + 1)); sleep 1; done
  [ -e /sys/class/net/rmnet_ipa0 ] || { log "no rmnet_ipa0"; return 1; }
  ip link set rmnet_ipa0 up
  flags=
  grep -q MAPv4 /sys/class/net/rmnet_ipa0/device/feature/rx_offload 2>/dev/null && flags="ingress-map-cksumv4"
  grep -q MAPv4 /sys/class/net/rmnet_ipa0/device/feature/tx_offload 2>/dev/null && flags="${flags:+$flags|}egress-map-cksumv4"
  for n in 0 1 2; do
    [ -e /sys/class/net/rmnet_data$n ] && continue
    qmicli -d qrtr://0 --link-add="iface=rmnet_ipa0,prefix=rmnet_data,mux-id=$((n + 1))${flags:+,flags=$flags}" >/dev/null 2>&1 \
      || ip link add link rmnet_ipa0 name rmnet_data$n type rmnet mux_id $((n + 1))
  done
  log "rmnet links ready"
}

i=0
while ! uim && [ $i -lt 300 ]; do i=$((i + 1)); sleep 1; done
uim || log "no UIM service on QRTR after 300 s"
uim && prep
systemctl --no-block start ofono.service

up=1
while :; do
  sleep 5
  if uim; then
    if [ $up = 0 ]; then
      log "modem back: preparing again, restarting ofono"
      prep
      systemctl restart ofono.service
      up=1
    fi
  else
    [ $up = 1 ] && log "modem down (no UIM service)"
    up=0
  fi
done
