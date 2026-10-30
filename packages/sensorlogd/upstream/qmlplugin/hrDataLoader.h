/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef HRDATALOADER_H
#define HRDATALOADER_H

#include <QObject>
#include <QDBusInterface>
#include <QPointF>
#include <QDate>
#include <QFileSystemWatcher>

class HrDataLoader : public QObject
{
    Q_OBJECT

public:
    explicit HrDataLoader();
    Q_INVOKABLE QVariant getDataForDate(QDate date);
    Q_INVOKABLE QVariant getTodayData();
    Q_INVOKABLE void triggerDaemonRecording();
    Q_INVOKABLE QVariant getDataFromTo(QDate date1, QDate date2);
    QList<QPointF> getRawDataForDate(QDate date);
signals:
    void dataChanged();
private:
    QDBusInterface *m_iface;
    QFileSystemWatcher *m_fileWatcher;
};

#endif // HRDATALOADER_H
