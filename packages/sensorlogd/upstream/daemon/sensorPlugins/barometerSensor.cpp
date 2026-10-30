/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */


#include <QDateTime>
#include <QTimer>
#include <QtSensors/QPressureSensor>
#include <QDebug>
#include <QString>
#include <MGConfItem>

#include "../logger.h"
#include "../../common.h"

#include "barometerSensor.h"

BarometerSensorPlugin::BarometerSensorPlugin(QObject *parent, int initInterval)  :
    QObject(parent){
    interval = initInterval;

    barometerSensor = new QPressureSensor(this);
    connect(barometerSensor,SIGNAL(readingChanged()),this,SLOT(finishRecording()));

    setupFilePath(sensorPathPrefix);
    setupFilePath(sensorPathPrefixCompensated);

    qDebug() << "barometer sensor is enabled. interval is (ms) " << interval;
    m_barometerOffsetGconf = new MGConfItem("/org/asteroidos/sensors/barometer-offset");
    recordIntervalTimer = new QTimer(this);
    connect(recordIntervalTimer,SIGNAL(timeout()),this,SLOT(triggerRecording()));
    recordIntervalTimer->setSingleShot(true);
    recordIntervalTimer->start(interval);
    lastRecordTime = QDateTime::currentDateTime();
}

void BarometerSensorPlugin::timeUpdate() {
    uint elapsed = QDateTime::currentMSecsSinceEpoch() - lastRecordTime.toMSecsSinceEpoch();
    qDebug() << "time until next steps recording" << recordIntervalTimer->remainingTime() << " elapsed = " << elapsed << " lastRecordTime " << lastRecordTime.toMSecsSinceEpoch();
    if (elapsed > interval) { //if too much time has passed, reset the timer and record
        triggerRecording();
        lastRecordTime = QDateTime::currentDateTime();
    } else { //otherwise, restart the timer and compensate for time spent in suspend
        recordIntervalTimer->start(interval - elapsed);
    }
}

void BarometerSensorPlugin::triggerRecording() {
    qDebug() << "barometer interval recording";
    recordIntervalTimer->start(interval);
    barometerSensor->start();
}

void BarometerSensorPlugin::finishRecording() {
    qDebug() << "barometer pressure update received";
    int pressure = barometerSensor->reading()->pressure();
    qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss") << " : " << pressure << barometerSensor->isActive();
    if (pressure == 0) {
        qDebug() << "barometer sensor accuracy insufficient. waiting.";
        return;
    }
    fileAddRecord(sensorPathPrefix,QString::number(pressure));
    fileAddRecord(sensorPathPrefixCompensated,QString::number(pressure - m_barometerOffsetGconf->value(0).toInt()));
    barometerSensor->stop();
}
