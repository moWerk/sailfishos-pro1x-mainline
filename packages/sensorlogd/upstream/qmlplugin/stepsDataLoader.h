/*/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef STEPSDATALOADER_H
#define STEPSDATALOADER_H

#include <QObject>
#include <QDBusInterface>
#include <QPointF>
#include <QFileSystemWatcher>

#include "../stepsource.h"

class StepsDataLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int todayTotal READ getTodayTotal() NOTIFY todayTotalChanged())

public:
    explicit StepsDataLoader();
    Q_INVOKABLE int getTotalForDate(QDate date);
    Q_INVOKABLE int getTodayTotal();
    Q_INVOKABLE QVariant getDataForDate(QDate date);
    Q_INVOKABLE QVariant getTodayData();
    Q_INVOKABLE void triggerDaemonRecording();
    Q_INVOKABLE QVariant getDataFromTo(QDate date1, QDate date2);
    QList<QPointF> getRawDataForDate(QDate date);
signals:
    void todayTotalChanged();
    void dataChanged();
private:
    QDBusInterface *m_iface;
    QFileSystemWatcher *m_fileWatcher;
    StepSource *m_stepcounterSensor;
};

#endif // STEPSDATALOADER_H
