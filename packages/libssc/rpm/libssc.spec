Name:       libssc
Version:    0.4.4
Release:    2
Summary:    Library to access the sensors of the Qualcomm Sensor Core
License:    GPL-3.0-or-later
URL:        https://codeberg.org/DylanVanAssche/libssc
Source0:    %{name}-%{version}.tar.bz2
# Applied in the tree (mb2 builds the directory as it is); listed for the record.
Patch1:     0001-Build-without-the-mock-server-and-tests.patch
Patch2:     0002-sensor-ignore-other-clients-discovery-while-waiting.patch
BuildRequires: meson >= 1.4.0
BuildRequires: ninja
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(qmi-glib) >= 1.33.4
BuildRequires: pkgconfig(libprotobuf-c)
BuildRequires: protobuf-c-devel
BuildRequires: protobuf-compiler

%description
GLib library talking QMI over QRTR to the Sensor Core (SSC) on the ADSP of Qualcomm SoCs:
accelerometer, gyroscope, magnetometer, compass, light and proximity. Used by the
F(x)tec Pro1-X mainline SailfishOS port (SM6115), where these sensors have no kernel driver.

%package devel
Summary:    Development files for libssc
Requires:   %{name} = %{version}-%{release}

%description devel
Headers and pkg-config file for libssc.

%package tools
Summary:    ssccli, a command line client for libssc
Requires:   %{name} = %{version}-%{release}

%description tools
ssccli reads one SSC sensor and prints its samples, for testing the sensor stack.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
%meson
%meson_build

%install
%meson_install

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license LICENSE
%{_libdir}/libssc.so.*

%files devel
%{_includedir}/libssc
%{_libdir}/libssc.so
%{_libdir}/pkgconfig/libssc.pc

%files tools
%{_bindir}/ssccli
