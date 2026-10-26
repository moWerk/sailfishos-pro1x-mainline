Name:       protobuf-c
Version:    1.5.2
Release:    1
Summary:    C implementation of Google Protocol Buffers
License:    BSD-2-Clause
URL:        https://github.com/protobuf-c/protobuf-c
Source0:    %{name}-%{version}.tar.bz2
BuildRequires: autoconf
BuildRequires: automake
BuildRequires: libtool
BuildRequires: pkgconfig(protobuf)
BuildRequires: protobuf-compiler

%description
Runtime library of protobuf-c. Not in the SailfishOS repositories; needed by libssc
(Qualcomm sensor core client) for the F(x)tec Pro1-X mainline port.

%package devel
Summary:    Development files and protoc-gen-c for protobuf-c
Requires:   %{name} = %{version}-%{release}
Requires:   protobuf-compiler

%description devel
Headers, pkg-config file and the protoc-gen-c code generator.

%prep
%autosetup -n %{name}-%{version}/upstream

%build
autoreconf -fi
%configure --disable-static
%make_build

%install
%make_install
rm -f %{buildroot}%{_libdir}/*.la

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%files
%license LICENSE
%{_libdir}/libprotobuf-c.so.*

%files devel
%{_bindir}/protoc-c
%{_bindir}/protoc-gen-c
%{_includedir}/protobuf-c
%{_includedir}/google/protobuf-c
%{_libdir}/libprotobuf-c.so
%{_libdir}/pkgconfig/libprotobuf-c.pc
