#!/bin/sh
# Bring-up network over the USB cable: the kernel's built-in g_cdc gadget gives usb0.
# Same address and DHCP range as the NixOS base (172.16.42.1, clients .11-.20).
for i in $(seq 1 30); do [ -e /sys/class/net/usb0 ] && break; sleep 1; done
ip link set usb0 up
ip addr add 172.16.42.1/24 dev usb0 2>/dev/null
# connman's firewall drops INPUT on interfaces it does not manage; usb0 is ours.
iptables -C INPUT -i usb0 -j ACCEPT 2>/dev/null || iptables -I INPUT -i usb0 -j ACCEPT
ip6tables -C INPUT -i usb0 -j ACCEPT 2>/dev/null || ip6tables -I INPUT -i usb0 -j ACCEPT
cat > /run/udhcpd-usb0.conf <<CONF
start 172.16.42.11
end 172.16.42.20
interface usb0
lease_file /run/udhcpd-usb0.leases
CONF
touch /run/udhcpd-usb0.leases
exec busybox udhcpd -f /run/udhcpd-usb0.conf
