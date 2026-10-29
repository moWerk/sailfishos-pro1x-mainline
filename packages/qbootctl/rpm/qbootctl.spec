Name:       qbootctl
Summary:    Qualcomm A/B slot control (mark boot successful, switch slots)
Version:    0.2.2
Release:    1
License:    GPL-3.0-only
URL:        https://github.com/linux-msm/qbootctl
Source0:    %{name}-%{version}.tar.gz

%description
qbootctl reads and writes the A/B slot attributes in the GPT of Qualcomm
devices, like Android's bootctrl HAL. Booting straight from the Android
bootloader (ABL), the running slot must be marked successful, or ABL stops
booting it once its retry count runs out. Upstream tag 0.2.2, built without
meson. scsi_bsg_ufs.h is vendored from the Linux 7.3 UAPI (the SDK headers predate it).

%prep
%setup -q -n %{name}-%{version}

%build
gcc %{optflags} -std=gnu11 -I src -I src/compat -o qbootctl src/qbootctl.c src/bootctrl_impl.c \
    src/gpt-utils.c src/ufs-bsg.c src/crc32.c

%install
install -Dm755 qbootctl %{buildroot}%{_bindir}/qbootctl

%files
%license src/LICENSE
%{_bindir}/qbootctl
