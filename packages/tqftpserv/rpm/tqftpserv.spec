Name:       tqftpserv
Version:    1.3
Release:    1
Summary:    TFTP server over QRTR (firmware files for Qualcomm remote processors)
License:    BSD-3-Clause
URL:        https://github.com/linux-msm/tqftpserv
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: meson
BuildRequires: ninja
BuildRequires: qrtr-devel
BuildRequires: pkgconfig(libzstd)
BuildRequires: pkgconfig(liblzma)
BuildRequires: pkgconfig(systemd)

%description
Serves firmware files to the modem and DSPs over QRTR. On the Pro1-X the
Wi-Fi firmware runs on the modem and needs it. Packaged for the F(x)tec Pro1-X
mainline SailfishOS port.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
%meson -Dsystemd-unit-prefix=%{_unitdir}
%meson_build

%install
%meson_install
# Never touch persist on this phone: upstream's populate-readwrite mounts it
# (read-only, but an ext4 mount can replay the journal). Not shipped.
rm -f %{buildroot}%{_bindir}/populate-readwrite.sh %{buildroot}%{_unitdir}/tqftpserv-populate-readwrite.service
%files
%license LICENSE
%{_bindir}/tqftpserv
%{_unitdir}/tqftpserv.service
