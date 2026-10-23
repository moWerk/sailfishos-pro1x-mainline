Name:       qrtr
Version:    1.2
Release:    1
Summary:    Qualcomm IPC Router (QRTR) library and tools
License:    BSD-3-Clause
URL:        https://github.com/linux-msm/qrtr
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: meson
BuildRequires: ninja

%description
Userspace library and tools for the Qualcomm IPC Router (AF_QIPCRTR).
Upstream linux-msm/qrtr, packaged for the F(x)tec Pro1-X mainline SailfishOS port.

%package devel
Summary:    Development files for qrtr
Requires:   %{name} = %{version}-%{release}

%description devel
Headers and library link for libqrtr.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
%meson
%meson_build

%install
%meson_install
# linux/qrtr.h (UAPI, GPL-2.0 WITH Linux-syscall-note) is newer than the SDK kernel headers
install -D -m 644 include/linux/qrtr.h %{buildroot}%{_includedir}/linux/qrtr.h

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license LICENSE
%{_bindir}/qrtr-*
%{_libdir}/libqrtr.so.*

%files devel
%{_includedir}/*.h
%{_includedir}/linux/qrtr.h
%{_libdir}/libqrtr.so
%{_libdir}/pkgconfig/qrtr.pc
