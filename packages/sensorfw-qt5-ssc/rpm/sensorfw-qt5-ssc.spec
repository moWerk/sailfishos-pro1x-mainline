Name:       sensorfw-qt5-ssc
Summary:    sensorfw adaptors for Qualcomm SSC sensors (libssc)
Version:    0.1
Release:    1
License:    LGPLv2
URL:        https://github.com/moWerk
Source0:    %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(sensord-qt5)
BuildRequires: pkgconfig(libssc)
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: sensorfw-qt5-devel
Requires:   sensorfw-qt5
Requires:   libssc

%description
Accelerometer, gyroscope, magnetometer, ambient light and proximity adaptor
plugins for sensord, reading the Snapdragon Sensor Core on the ADSP through
libssc (QRTR/QMI). For mainline kernels where these sensors have no IIO device.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

%files
%{_libdir}/sensord-qt5/libssc*adaptor-qt5.so
