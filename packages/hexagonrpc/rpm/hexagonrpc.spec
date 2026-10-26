Name:       hexagonrpc
# linux-msm/hexagonrpc pull request 27 head 40e5041 ("Support for writing files", closed
# unmerged) + nyymi609's sns_registry interface, as the fxtec-pro1x-nixos sensors module uses it
Version:    0.5.0+git40e5041
Release:    1
Summary:    FastRPC daemon serving files and the sensor registry to the Qualcomm ADSP
License:    GPL-3.0-or-later
URL:        https://github.com/linux-msm/hexagonrpc
Source0:    %{name}-%{version}.tar.bz2
# Applied in the tree (mb2 builds the directory as it is); listed for the record.
# From fxtec-pro1x-nixos packages/hexagonrpc-sns-registry.patch (nyymi609).
Patch1:     hexagonrpc-sns-registry.patch
# include/misc/fastrpc.h: Linux UAPI header (v6.18, GPL-2.0 WITH Linux-syscall-note), newer than
# the SDK kernel headers; copied into the tree.
BuildRequires: meson
BuildRequires: ninja

%description
hexagonrpcd answers the FastRPC calls the ADSP makes while it brings up its Sensor Core
(SSC): it serves the sensor configuration and registry files and the sns_registry
properties. Without it the accelerometer, gyroscope, magnetometer, light and proximity
sensors of the F(x)tec Pro1-X (SM6115) never appear. The systemd unit and the device data
come with the Pro1-X adaptation, not with this package.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
%meson
%meson_build

%install
%meson_install
# The upstream units go to libdir/systemd (lib64 here) and run without the registry path
# the Pro1-X needs; the adaptation ships its own unit.
rm -rf %{buildroot}%{_libdir}/systemd

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license COPYING
%{_bindir}/hexagonrpcd
%{_libdir}/libhexagonrpc.so*
%{_libexecdir}/hexagonrpc
%{_mandir}/man1/hexagonrpcd.1*
