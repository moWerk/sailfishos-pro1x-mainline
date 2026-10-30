# sensorlogd: the AsteroidOS step logger on SailfishOS

[asteroid-sensorlogd](https://github.com/AsteroidOS/asteroid-sensorlogd) by **Arseniy Movshev
(dodoradio)**, GPL-3.0-or-later. The design, the daily log format, the date-change handling and
the QML module (`org.asteroid.sensorlogd`: today's total, per-day data, settings) are his; this
package only builds it on SailfishOS. `upstream/` is his tree at 19a660f (the last Qt 5 revision,
2026-04-15); our changes are in `rpm/0001-...patch` (also applied in the tree) and are meant to go
back upstream.

What the port changes:
- **Step source**: SailfishOS's QtSensors 5.6 has no step counter, so `stepsource.h` reads sensorfw's
  stepcountersensor through libsensorclient (`SENSORLOGD_SENSORFW`), with standby override so the
  sensor stays open while the display is off. The Pro1-X feeds it from the Sensor Core's pedometer
  (`packages/sensorfw-qt5-ssc`, libssc patch 0003).
- **Optional loggers**: heart rate (QHrmSensor, AsteroidOS's QtSensors only) and barometer are
  build options, off here.
- **Counter semantics** (`SENSORLOGD_COUNTER_RESETS_ON_OPEN`): every daemon start continues from the
  day's last record, and a counter that starts over mid-run is followed instead of dropping records.
- Records carry their own timestamp; Qt 5.6 build fixes.

Runs as the user service `asteroid-sensorlogd` with the user session; logs go to
`~/.asteroid-sensorlogd/stepCounter/<date>.log` (`<epoch seconds>:<steps today>`), one record per
10 minutes of awake time and at display-on, as upstream. It never wakes the phone itself: the Sensor
Core counts while the phone sleeps, the next record picks the steps up.

Tested 2026-10-30 on the Pro1-X: the daemon starts, opens the counter and writes records. Not yet
tested: a full suspend with steps in between, and a day change.
