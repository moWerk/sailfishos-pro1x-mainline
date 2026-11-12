Name: geoclue-provider-hybris-qmi
Version: 0.2.24
Release: 3
Provides: geoclue-provider-hybris = %{version}-%{release}
Conflicts: geoclue-provider-hybris-hal
Conflicts: geoclue-provider-hybris-binder

# hardcoded build command, can be autodetected
%define qmake_command %qmake5 qmi/qmilocationbackend.pro

# QRTR sockets need no privileges: no suid bit (with_suid stays unset)

%include rpm/geoclue-providers-hybris.inc
