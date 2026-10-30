# Kernel update package for the Pro1-X mainline port: the boot image (kernel + DTB +
# initramfs) and the kernel's modules, built elsewhere (kernel/build-kernel.sh on the
# build host, scripts/build-bootimg.sh) and put into input/ by prepare-input.sh.
# mb2 builds in tree; nothing is compiled here.
%define kver 7.3.0-qx1050

Name:       kernel-qx1050
Summary:    Linux kernel, modules and boot image for the F(x)tec Pro1-X
Version:    7.3.0
Release:    1
License:    GPL-2.0-only
URL:        https://github.com/moWerk/sailfishos-pro1x-mainline
Source0:    %{name}-%{version}.tar.gz
Requires(post): oneshot
Requires:   kmod
Provides:   kernel = %{version}

%description
The port's own kernel (Linux %{kver}, patches in kernel/patches-7.3/) as an Android
boot image for the bootloader (ABL), with its modules. Installing it on the phone
schedules /var/lib/platform-updates/flash-bootimg.sh for the next boot, as hybris
ports do with droid-hal-img-boot: SailfishOS's preinit writes the image to the
running boot slot before systemd starts and reboots into it.

%prep
%setup -q -n %{name}-%{version}

%build

%install
install -Dm644 input/sfos-boot.img %{buildroot}/boot/sfos-boot.img
install -Dm755 flash-bootimg.sh %{buildroot}/var/lib/platform-updates/flash-bootimg.sh
mkdir -p %{buildroot}/lib/modules
# modules.tar holds lib/modules/%{kver}/ as written by modules_install
tar --no-same-owner -xf input/modules.tar -C %{buildroot}
test -d %{buildroot}/lib/modules/%{kver}
rm -f %{buildroot}/lib/modules/%{kver}/build %{buildroot}/lib/modules/%{kver}/source

%post
/sbin/depmod %{kver} || :
# Only on the phone, not when the package goes into an image (the image's boot
# image is flashed with it).
if grep -q "fxtec,pro1x" /sys/firmware/devicetree/base/compatible 2>/dev/null; then
  add-preinit-oneshot /var/lib/platform-updates/flash-bootimg.sh || :
fi

%files
/boot/sfos-boot.img
/var/lib/platform-updates/flash-bootimg.sh
/lib/modules/%{kver}
