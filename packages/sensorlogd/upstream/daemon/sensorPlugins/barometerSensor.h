/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef BAROMETERSENSOR_H
#define BAROMETERSENSOR_H

#include <QObject>
#include <QDateTime>
#include <QDBusInterface>
#include <QTimer>
#include <QSettings>
#include <MGConfItem>

#include <QtSensors/QPressureSensor>

class BarometerSensorPlugin : public QObject {
    Q_OBJECT
public:
    explicit BarometerSensorPlugin(QObject* parent = 0, int initInterval = 600000);
    virtual ~BarometerSensorPlugin() {};

    void timeUpdate();

public slots:
    void triggerRecording();

private slots:
    void finishRecording();

private:
    QDateTime lastRecordTime;
    int interval;
    QTimer *recordIntervalTimer;
    QPressureSensor *barometerSensor;
    QSettings *m_settings;
    MGConfItem *m_barometerOffsetGconf;

    const QString sensorPathPrefix = "barometer-raw";
    const QString sensorPathPrefixCompensated = "barometer-compensated";
};

#endif // BAROMETERSENSOR_H
