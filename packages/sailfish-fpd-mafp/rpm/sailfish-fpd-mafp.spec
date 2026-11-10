Name:       sailfish-fpd-mafp
Summary:    Fingerprint daemon for the F(x)tec Pro1X on a mainline kernel
Version:    0.1.0
Release:    1
License:    GPL-3.0-or-later
URL:        https://github.com/moWerk/sailfishos-pro1x-mainline
Source0:    %{name}-%{version}.tar.bz2
Requires:   qsee-supplicant
Requires:   systemd
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
Provides:   sailfish-fpd

%description
Implements org.sailfishos.fingerprint1 (the API of Jolla's sailfish-fpd, as
sailfish-devicelock-fpd and the Settings fingerprint pages use it) on top of
the Pro1X's MicroArray sensor, driven by the stock mafp trusted application
through the QSEECOM TEE driver and qsee-supplicant. The D-Bus side follows
sailfish-fpd-community (sailfishos-open).

%prep
%autosetup -n %{name}-%{version}

%build
%qmake5
%make_build

%install
%qmake5_install
mkdir -p %{buildroot}%{_unitdir}/multi-user.target.wants
ln -s ../%{name}.service %{buildroot}%{_unitdir}/multi-user.target.wants/%{name}.service

%files
%defattr(-,root,root,-)
%license LICENSE
%{_bindir}/%{name}
%{_sysconfdir}/dbus-1/system.d/org.sailfishos.fingerprint1.conf
%{_unitdir}/%{name}.service
%{_unitdir}/multi-user.target.wants/%{name}.service

%post
systemctl daemon-reload || :
systemctl reload-or-restart %{name}.service || :

%preun
if [ "$1" = "0" ]; then
    systemctl stop %{name}.service || :
fi

%postun
systemctl daemon-reload || :
