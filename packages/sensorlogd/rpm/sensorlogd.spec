Name:       sensorlogd
Version:    1.0.0+git19a660f
Release:    1
Summary:    Step logger daemon and QML module (port of AsteroidOS's sensorlogd)
License:    GPL-3.0-or-later
URL:        https://github.com/AsteroidOS/asteroid-sensorlogd
Source0:    %{name}-%{version}.tar.bz2
# Applied in the tree (mb2 builds the directory as it is); listed for the record.
Patch1:     0001-Read-steps-from-sensorfw-and-make-the-other-sensors-optional.patch
BuildRequires: cmake
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Qml)
BuildRequires: pkgconfig(Qt5Quick)
BuildRequires: pkgconfig(Qt5Sensors)
BuildRequires: pkgconfig(sensord-qt5)
Requires:   sensorfw-qt5
Requires:   systemd

%description
sensorlogd keeps the step counter open and writes the step count to one log file per day
(~/.asteroid-sensorlogd/stepCounter/), so steps taken while the phone sleeps are counted by
the sensor hub and recorded when it wakes. The QML module org.asteroid.sensorlogd reads the
logs for apps. Written for AsteroidOS by Arseniy Movshev (dodoradio); this build reads the step
counter from sensorfw (SailfishOS has no QtSensors step counter) and leaves out the heart rate
and barometer loggers. Packaged for the F(x)tec Pro1-X mainline SailfishOS port, where the
Qualcomm Sensor Core counts the steps (sensorfw-qt5-ssc).

%prep
%autosetup -n %{name}-%{version}/upstream

%build
%cmake . -DSENSORLOGD_HEARTRATE=OFF -DSENSORLOGD_BAROMETER=OFF -DSENSORLOGD_SENSORFW=ON \
    -DSENSORLOGD_COUNTER_RESETS_ON_OPEN=ON -DINSTALL_QML_IMPORT_DIR=%{_libdir}/qt5/qml
%make_build

%install
%make_install
# SailfishOS starts user services with the user session, not basic.target
sed -i 's/^WantedBy=basic.target$/WantedBy=user-session.target/; s/^Description=Sensor logger for AsteroidOS$/Description=Step logger (sensorlogd)/' \
    %{buildroot}%{_userunitdir}/asteroid-sensorlogd.service
mkdir -p %{buildroot}%{_userunitdir}/user-session.target.wants
ln -s ../asteroid-sensorlogd.service %{buildroot}%{_userunitdir}/user-session.target.wants/

%files
%license LICENSE
%{_bindir}/sensorlogd
%{_userunitdir}/asteroid-sensorlogd.service
%{_userunitdir}/user-session.target.wants/asteroid-sensorlogd.service
%{_libdir}/qt5/qml/org/asteroid/sensorlogd
