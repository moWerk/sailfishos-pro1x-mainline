Name:       rmtfs
Version:    1.3
Release:    1
Summary:    Qualcomm remote filesystem service (read-only use)
License:    BSD-3-Clause
URL:        https://github.com/linux-msm/rmtfs
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: pkgconfig(libudev)
BuildRequires: qrtr-devel

%description
Serves the modem's EFS partitions (modemst1/2, fsg, fsc) over QMI.
The service runs as "rmtfs -r -P -s": -r is read-only, the modem's partitions
on the phone are never written. Packaged for the F(x)tec Pro1-X mainline SailfishOS port.

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
