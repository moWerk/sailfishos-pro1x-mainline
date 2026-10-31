Name:       rmtfs
Version:    1.3
Release:    2
Summary:    Qualcomm remote filesystem service
License:    BSD-3-Clause
URL:        https://github.com/linux-msm/rmtfs
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(libudev)
BuildRequires: qrtr-devel

%description
Serves the modem's EFS (modemst1/2, fsg, fsc) over QMI.
On the Pro1-X port the service runs from files ("rmtfs -o /var/lib/rmtfs -s",
droid-config's rmtfs.service.d): qx1050-rmtfs-seed copies the four partitions
into /var/lib/rmtfs once, and the modem's writes land there, never on the
partitions. Packaged for the F(x)tec Pro1-X mainline SailfishOS port.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
%make_build prefix=%{_prefix}

%install
make install DESTDIR=%{buildroot} prefix=%{_prefix} servicedir=%{_unitdir} rulesdir=%{_udevrulesdir}

%files
%license LICENSE
%{_bindir}/rmtfs
%{_unitdir}/rmtfs.service
%{_unitdir}/rmtfs-dir.service
%{_udevrulesdir}/rmtfs.rules
