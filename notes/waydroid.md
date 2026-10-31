# Waydroid on the mainline Pro1-X (plan, 2026-10-31)

Queued for the end of the polish run. Nothing is installed yet.

## Kernel: already there

Checked in the kernel m config:
- binder: `CONFIG_ANDROID_BINDER_IPC=y`, `CONFIG_ANDROID_BINDERFS=y`, devices
  `binder,hwbinder,vndbinder`;
- PSI, user/pid/net namespaces, the cgroups (memcg, freezer, device, pids, bpf);
- networking modules: veth, bridge, nf_tables, MASQUERADE, IP NAT, addrtype; FUSE.

Missing: `CONFIG_UHID` (only for UHID-based input). Ashmem is not needed: current Waydroid
images use memfd.

## Userspace (Chum)

- Chum carries waydroid (1.4.3 for 5.1+), waydroid-settings, waydroid-sensors,
  python3-gbinder and dnsmasq. Hybris ports add `waydroid-gbinder-config-hybris`.
- On mainline the host binder is the plain upstream one. libgbinder needs a config without
  the hybris service-manager quirks, presumably the generic `/etc/gbinder.d` setup Waydroid
  uses on desktop distributions. To check: is there a mainline variant on Chum, or does it
  need a small config package of our own?
- GPU: freedreno/Mesa on the host. The Waydroid image needs a Mesa/minigbm (GBM) build for
  the Adreno 610. The LineageOS-based images ship one; check `ro.hardware.gralloc=gbm` and
  `ro.hardware.egl=mesa`.
- Wayland: lipstick is the compositor; on SailfishOS, waydroid-runner shows the Android
  session in a window.

## Order when it comes up

1. Install from Chum: waydroid, waydroid-settings, python3-gbinder, dnsmasq. Leave out the
   hybris gbinder config.
2. Run `waydroid init` (download the LineageOS GAPPS-free image, on /home, not /), then
   `waydroid session start`, and look at the logcat output.
3. Typical trouble spots: binderfs mount, the gbinder config, GPU (fall back to software
   rendering with swiftshader to separate it from the rest), networking (waydroid0 bridge,
   dnsmasq).
