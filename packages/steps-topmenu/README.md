# steps-topmenu: today's steps in the SailfishOS top menu

A top menu button (Settings > Top menu > Steps) that shows today's step count under its icon and
counts up live while the phone is awake. A tap asks sensorlogd to write a record now.

Install: `entries/steps-topmenu.json` to `/usr/share/jolla-settings/entries/`,
`qml/StepsToggle.qml` to `/usr/share/steps-topmenu/`, restart lipstick, add the button in
Settings > Top menu. Needs `packages/sensorlogd` (its QML module `org.asteroid.sensorlogd`). Not
an RPM yet.

The chain behind it (2026-10-30, F(x)tec Pro1-X, mainline 7.3, SailfishOS 5.2):

1. The Qualcomm Sensor Core on the ADSP runs a pedometer on the accelerometer, also while the
   application processor sleeps.
2. libssc gains a pedometer sensor (`packages/libssc/rpm/0003-*`, for Dylan Van Assche's libssc).
3. sensorfw-qt5-ssc gains `sscstepcounteradaptor`, registered as sensorfw's `stepcounteradaptor`.
   sensorfw's own `stepcountersensor` has existed for years; the default config hides it and
   SailfishOS's QtSensors 5.6 has no class for it, so no app could use it.
4. sensorlogd, the AsteroidOS step logger by Arseniy Movshev (dodoradio), ported to read sensorfw
   (`packages/sensorlogd`), keeps the counter open and logs the steps per day.
5. This button: a `SettingsToggle` on sensorlogd's QML module.

Everything from step 3 up is device independent: any SailfishOS device with a sensorfw step
counter adaptor can use 4 and 5 (hybris devices have `hybrisstepcounteradaptor` for an Android
HAL step counter; not tried).

Tested: steps counted live in the top menu on the Pro1-X.
