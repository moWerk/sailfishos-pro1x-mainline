Name:       qx1050-voicecall
Summary:    F(x)tec Pro1-X call audio: q6voice session helper for the Voice Call UCM verb
Version:    0.1
Release:    6
License:    MIT
URL:        https://github.com/moWerk
Source0:    %{name}-%{version}.tar.gz
BuildRequires: alsa-lib-devel
Requires:   bash
Requires:   alsa-utils
Requires:   gstreamer1.0-tools
Requires:   gstreamer1.0-plugins-good

%description
pro1x-voicepcm keeps the q6voice VoiceMMode1 hostless PCM open while a call is
up; pro1x-voice-call starts it from the "Voice Call" UCM verb when ofono reports
a call. Ported from fxtec-pro1x-nixos device/audio.

%prep
%setup -q -n %{name}-%{version}

%build
gcc %{optflags} -o pro1x-voicepcm src/voicepcm.c -lasound

%install
install -Dm755 pro1x-voicepcm %{buildroot}%{_bindir}/pro1x-voicepcm
install -Dm755 src/pro1x-voice-call %{buildroot}%{_bindir}/pro1x-voice-call

%files
%{_bindir}/pro1x-voicepcm
%{_bindir}/pro1x-voice-call
