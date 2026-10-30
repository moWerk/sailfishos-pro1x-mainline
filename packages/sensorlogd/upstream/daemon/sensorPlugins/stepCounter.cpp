/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QString>
#include <QTime>
#include <QTimer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QSettings>
#include <time.h>

#include "../logger.h"
#include "../../common.h"

#include "stepCounter.h"

StepCounterPlugin::StepCounterPlugin(QObject* parent, int initInterval, bool daemonFresh)
    : QObject(parent)
{
    interval = initInterval;

    stepcounterSensor = new StepSource(this);
    stepcounterSensor->start();

    qDebug() << "step counter sensor is enabled. interval is (ms) " << interval;
    recordIntervalTimer = new QTimer(this);
    connect(recordIntervalTimer, SIGNAL(timeout()), this, SLOT(triggerRecording()));
    recordIntervalTimer->setSingleShot(true);
    recordIntervalTimer->start(interval);
    QDateTime currDateTime = QDateTime::currentDateTime();

    setupFilePath(sensorPathPrefix);
    // Wait for the first reading. Spinning without the event loop works with QtSensors'
    // synchronous start but never sees sensorfw's first sample, which arrives as a signal.
    QElapsedTimer waited;
    waited.start();
    while (!stepcounterSensor->isActive() && waited.elapsed() < 10000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
#ifdef SENSORLOGD_COUNTER_RESETS_ON_OPEN
    // The Qualcomm Sensor Core counts from 0 whenever the counter is opened, not since boot:
    // every daemon start continues from the last record, like the first start of a boot.
    daemonFresh = true;
#endif
    m_settings = new QSettings("asteroid", "sensorlogd");

    if (dayFileExists(sensorPathPrefix) && daemonFresh) {
        QStringList lastLineData = fileGetPrevRecord(sensorPathPrefix);
        lastRecordTime = QDateTime::currentDateTime();
        m_stepsOffset = stepcounterSensor->steps() - lastLineData[1].toInt(); // we reset the step count, ignoring all steps that may have been taken while the daemon isn't running. Devices which count steps independently will need custom code.
    } else if (dayFileExists) {
        lastRecordTime = QDateTime::currentDateTime();
        m_stepsOffset = m_settings->value("StepCounterPrivate/stepsOffset", 0).toInt();
    }
    m_settings->setValue("StepCounterPrivate/stepsOffset", m_stepsOffset);
}

void StepCounterPlugin::timeUpdate()
{
    QDateTime currDateTime = QDateTime::currentDateTime();
    uint elapsed = currDateTime.toMSecsSinceEpoch() - lastRecordTime.toMSecsSinceEpoch();
    qDebug() << "time until next steps recording" << recordIntervalTimer->remainingTime() << " elapsed = " << elapsed << " lastRecordTime " << lastRecordTime.toMSecsSinceEpoch();
    if (elapsed > interval) { // if too much time has passed, reset the timer and record
        triggerRecording();
    } else { // otherwise, restart the timer and compensate for time spent in suspend
        recordIntervalTimer->start(interval - elapsed);
    }
}

void StepCounterPlugin::triggerRecording()
{
    qDebug() << "stepcounter interval recording";
    int rawSteps = stepcounterSensor->steps();
#ifdef SENSORLOGD_COUNTER_RESETS_ON_OPEN
    if (rawSteps < m_lastRawSteps && dayFileExists(sensorPathPrefix)) {
        // The counter started over (sensord restarted and reopened it): continue from the last record
        m_stepsOffset = rawSteps - fileGetPrevRecord(sensorPathPrefix)[1].toInt();
        m_settings->setValue("StepCounterPrivate/stepsOffset", m_stepsOffset);
        qDebug() << "step counter restarted, offset is now" << m_stepsOffset;
    }
    m_lastRawSteps = rawSteps;
#endif
    if (lastRecordTime.date() != QDate::currentDate()) { // ok, so the date has changed.
        if (dayFileExists(sensorPathPrefix)) { // this likely means we have just had a system time change
            QStringList lastLineData = fileGetPrevRecord(sensorPathPrefix);
            m_stepsOffset = rawSteps - lastLineData[1].toInt(); // we scrap the current steps value, recover the last reading from today's file and count from there
                // The reset is necessary because asteroid doesn't have robust time when it boots up, it takes a while for that to happen. So, instead of dealing with that, we just reset the counter every time there's a date change
        } else if (lastRecordTime.date() == QDate::currentDate().addDays(-1)) { // this means that a midnight just passed
            int steps = rawSteps - m_stepsOffset;
            fileAddRecord(sensorPathPrefix, QString::number(steps), QDateTime(QDate::currentDate().addDays(-1), QTime(23, 59, 59))); // this writes the current step count to the last second of the previous day
            m_stepsOffset = rawSteps; // and then we 'reset' the step counter
        }
        m_settings->setValue("StepCounterPrivate/stepsOffset", m_stepsOffset);
        qDebug() << "date change detected. Step counter value is " << rawSteps << " and offset is " << m_stepsOffset;
    } else {
        int steps = rawSteps - m_stepsOffset;
        qDebug() << QDateTime::currentDateTime().toString("hh:mm:ss") << " : " << steps << stepcounterSensor->isActive();
        // we probably ought to do some error checking here
        if (steps < fileGetPrevRecord(sensorPathPrefix)[1].toInt()) {
            qDebug() << "error: offset value is less than previous value - not recording"; // this is an error condition which is either caused by a bad offset value or by the sensor being slow and returning a bad value
            return;
        }
        fileAddRecord(sensorPathPrefix, QString::number(steps));
    }
    lastRecordTime = QDateTime::currentDateTime();
}
