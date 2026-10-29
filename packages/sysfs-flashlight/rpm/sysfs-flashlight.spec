Name:       sysfs-flashlight
Summary:    Flashlight D-Bus service for LED class torch devices
Version:    0.1
Release:    1
License:    Apache-2.0
URL:        https://github.com/moWerk/sailfishos-pro1x-mainline
Source0:    %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(libresourceqt5)
Provides:   sailfish-flashlight-provider
Conflicts:  droidmedia-flashlight

%description
The org.sailfish.flashlight.provider session service behind the top-menu
flashlight toggle (jolla-settings-system-flashlight), for native ports without
droidmedia: Jolla's droidmedia-flashlight with the Android camera service
replaced by LED class devices. The torch LEDs and their brightness are set in
/etc/sysfs-flashlight.conf; the port makes their brightness files writable
for the user session.

%prep
%setup -q -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install

%files
%license LICENSE
%{_bindir}/sysfs-flashlight
%{_datadir}/dbus-1/services/org.sailfish.flashlight.provider.service
