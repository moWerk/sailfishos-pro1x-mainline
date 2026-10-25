Name:       libcamera
Summary:    A library to support complex camera ISPs
Version:    0.7.2
Release:    4.qx1050
License:    LGPLv2+
URL:        https://libcamera.org
Source:     %{name}-%{version}.tar.bz2
Patch1:     0001-process-pidfd-clone3-compat-for-4.4-headers.patch
# Pro1-X sensors, from nyymi609/fxtec-pro1x-nixos device/libcamera
Patch2:     0002-imx582-sensor-helper-and-properties.patch
Patch3:     0003-s5k4h7-sensor-helper-and-properties.patch
# Autofocus: Milan Zamazal's RFC "Autofocus basics for software ISP" (patchwork series 6276,
# 2026-10-21) backported to 0.7.2, plus the sharpness statistic it leaves open (0010)
Patch4:    0004-libcamera-software_isp-Let-a-single-point-decide-abo.patch
Patch5:    0005-ipa-ipu3-Move-non-specific-parts-of-Af-to-libipa.patch
Patch6:    0006-libcamera-software_isp-Add-variance-to-stats.patch
Patch7:    0007-ipa-softisp-Add-Af-algorithm.patch
Patch8:    0008-libcamera-software_isp-Add-autofocus-controls.patch
Patch9:    0009-ipa-libipa-Use-autofocus-limits-if-available.patch
Patch10:    0010-software_isp-Compute-an-autofocus-sharpness-value.patch
Patch11:    0011-ipa-libipa-af-Make-the-scan-parameters-configurable.patch
Source1:    imx582.yaml
Source2:    s5k4h7.yaml

BuildRequires: boost-devel
BuildRequires: cmake
BuildRequires: meson
BuildRequires: ninja
BuildRequires: openssl
BuildRequires: python3-jinja2
BuildRequires: python3-ply
BuildRequires: python3-yaml
BuildRequires: pkgconfig(gstreamer-1.0)
BuildRequires: pkgconfig(gstreamer-plugins-base-1.0)
BuildRequires: libevent-devel
BuildRequires: pkgconfig(libjpeg)
BuildRequires: pkgconfig(libtiff-4)
BuildRequires: libv4l-devel
BuildRequires: pkgconfig(libudev)
BuildRequires: pkgconfig(openssl)
BuildRequires: pkgconfig(yaml-0.1)

%description
libcamera is a library that deals with heavy hardware image processing
operations of complex camera devices that are shared between the linux
host all while allowing offload of certain aspects to the control of
complex camera hardware such as ISPs.

Hardware support includes USB UVC cameras, libv4l cameras as well as more
complex ISPs (Image Signal Processor).

%package devel
Summary: Development files for libcamera
Requires: %{name}%{?_isa} = %{version}-%{release}

%description devel
Files for development with %{name}.

%package tools
Summary: Tools for libcamera
Requires: %{name}%{?_isa} = %{version}-%{release}

%description tools
Tools for %{name}.

%prep
%autosetup -p1 -n %{name}-%{version}
# upstream v0.7.2 unpatched; qcam/lc-compliance/tests disabled (Qt widgets, gtest not in the target)

%build
export LDFLAGS="-Wl,--copy-dt-needed-entries"
%meson \
    -Ddocumentation=disabled \
    -Dgstreamer=enabled \
    -Dpycamera=disabled \
    -Dtracing=disabled \
    -Dv4l2=true \
    -Dqcam=disabled \
    -Dlc-compliance=disabled \
    -Dtest=false \
    -Dcam=enabled \
    -Dpipelines=simple,uvcvideo \
    -Dipas=simple \
    -Dlibunwind=disabled \
    -Dlibdw=disabled \
    -Dcam-output-sdl2=disabled

%meson_build

%install
%meson_install
install -Dm644 %{SOURCE1} %{buildroot}%{_datadir}/libcamera/ipa/simple/imx582.yaml
install -Dm644 %{SOURCE2} %{buildroot}%{_datadir}/libcamera/ipa/simple/s5k4h7.yaml

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%license COPYING.rst LICENSES/LGPL-2.1-or-later.txt
%{_datadir}/libcamera/
%{_libdir}/gstreamer-1.0/libgstlibcamera.so
%{_libdir}/libcamera/
%{_libdir}/libcamera*.so.*
%{_libexecdir}/libcamera/

%files devel
%{_includedir}/libcamera
%{_libdir}/libcamera*.so
%{_libdir}/pkgconfig/*.pc

%files tools
%{_bindir}/cam
%{_bindir}/libcamerify
%{_bindir}/libcamera-bug-report
