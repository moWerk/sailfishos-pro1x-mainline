Name:           qsee-supplicant
Version:        0.1.1
Release:        1
Summary:        Userspace services for Qualcomm QSEECOM trusted applications
License:        BSD-2-Clause AND BSD-3-Clause-Clear
URL:            https://github.com/wrobelda/qsee-supplicant
Source0:        %{name}-%{version}.tar.bz2
BuildRequires:  gcc
BuildRequires:  make
Requires:       systemd

%description
Dawid Wróbel's listener supplicant (FS and GPFS listeners) and per-application
loader for trusted applications behind Qualcomm's legacy QSEECOM interface,
through the Linux TEE subsystem (the out-of-tree qseecomtee driver).
Packaged for the F(x)tec Pro1-X mainline SailfishOS port from upstream
commit 36e0668 (the tree in upstream/, unchanged). The fingerprint daemon
needs qsee-supplicant.service and qsee-app-loader@mafp.service.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
# uapi/: tee.h (v7.3) and openat2.h, newer than the SDK target's kernel-headers, whose
# syscall table also predates openat2 (437 on arm64, asm-generic numbering)
%make_build CPPFLAGS="-D_GNU_SOURCE -Iinclude -I../uapi -DSYS_openat2=437" qsee-supplicant qsee-app-loader

%check
%make_build CPPFLAGS="-D_GNU_SOURCE -Iinclude -I../uapi -DSYS_openat2=437" check

%install
%{__make} DESTDIR=%{buildroot} PREFIX=%{_prefix} SBINDIR=%{_sbindir} \
           UNITDIR=%{_unitdir} DOCDIR=%{_docdir}/%{name} \
           install-bin install-systemd install-doc

%post
systemctl daemon-reload || :

%preun
if [ "$1" = "0" ]; then
    systemctl stop 'qsee-app-loader@*.service' qsee-supplicant.service || :
fi

%postun
systemctl daemon-reload || :

%files
%license LICENSE LICENSES/BSD-3-Clause-Clear.txt
%{_sbindir}/qsee-supplicant
%{_sbindir}/qsee-app-loader
%{_unitdir}/qsee-supplicant.service
%{_unitdir}/qsee-app-loader@.service
%{_docdir}/%{name}/README.md
