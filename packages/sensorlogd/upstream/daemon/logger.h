/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QDateTime>
#include <QDate>
#include <QDBusInterface>
#include <QTimer>
#include <QString>
#include <QSettings>
#include <QDBusAbstractAdaptor>

#include "sensorPlugins/stepCounter.h"
#ifdef SENSORLOGD_HEARTRATE
#include "sensorPlugins/heartrateSensor.h"
#endif
#ifdef SENSORLOGD_BAROMETER
#include "sensorPlugins/barometerSensor.h"
#endif

class Logger : public QObject
{
    Q_OBJECT
public:
    explicit Logger(QObject *parent = 0);
    virtual ~Logger() {};
public slots:
    void setup();
    void resetup();
    void triggerRecording();
private slots:
    void displayOn(QString displayState);

private:
    QDBusInterface *m_iface;
    bool heartrateSensorEnabled = false;
#ifdef SENSORLOGD_HEARTRATE
    HeartrateSensorPlugin *m_heartrateSensor;
#endif
    bool stepCounterEnabled = false;
    StepCounterPlugin *m_stepCounter;
    bool barometerEnabled = false;
#ifdef SENSORLOGD_BAROMETER
    BarometerSensorPlugin *m_barometerSensor;
#endif
    bool daemonFresh;
    QSettings *settings;
    bool getDaemonFresh();

};
    bool dayFileExists(QString sensorPrefix, QDateTime date = QDateTime::currentDateTime()); //check if today has a log file for the given sensor
    QStringList fileGetPrevRecord(QString sensorPrefix, QDateTime recordTime = QDateTime::currentDateTime()); //works backwards to find the last record in today's file before the given time - returns nothing if no file is found.

#endif // LOGGER_H
