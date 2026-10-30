/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QSettings>
#include <QDBusInterface>
#include <QPointF>
#include <QFileSystemWatcher>

#include "barometerDataLoader.h"
#include "../common.h"

BarometerDataLoader::BarometerDataLoader() : QObject()
{
    m_iface = new QDBusInterface("org.asteroid.sensorlogd.logger","/org/asteroid/sensorlogd/logger","", QDBusConnection::sessionBus(), this);
    if (!m_iface->isValid()) {
        qDebug()<<"interface is not valid";
    } else {
        qDebug()<<"interface is valid";
    }
    m_fileWatcher = new QFileSystemWatcher();
    m_fileWatcher->addPath(sensorDirPath("barometer-compensated"));
    QObject::connect(m_fileWatcher,SIGNAL(directoryChanged(const QString)),this,SIGNAL(dataChanged()));
    QObject::connect(m_fileWatcher,SIGNAL(fileChanged(const QString)),this,SIGNAL(dataChanged()));
}

QVariant BarometerDataLoader::getTodayData() {
    return getDataForDate(QDate::currentDate());
}

QVariant BarometerDataLoader::getDataForDate(QDate date) {
    return QVariant::fromValue(getRawDataForDate(date));
}

QList<QPointF> BarometerDataLoader::getRawDataForDate(QDate date) {
    QList<QPointF> m_filedata;
    QString path = fileNameForDate(date, "barometer-compensated");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "failed to open file";
        return m_filedata;
    }
    if (!m_fileWatcher->files().contains(path)) {
        m_fileWatcher->addPath(path);
    }
    QTextStream inStream(&file);
    QString line;
    while (!inStream.atEnd()) {
        line = inStream.readLine();
        QPointF point;
        point.setX(line.split(":")[0].toInt());
        point.setY(line.split(":")[1].toInt());
        m_filedata.append(point);
    }
    file.close();
    return m_filedata;
}

QVariant BarometerDataLoader::getDataFromTo(QDate date1, QDate date2) {
    QList<QPointF> filedata;
    if (date1 > date2) {
        QDate temp = date1;
        date1 = date2;
        date2 = temp;
    }
    for (QDate i = date1; i <= date2; i = i.addDays(1)) {
        qDebug() << "currently loading " << i;
        QList<QPointF> data = getRawDataForDate(i);
        if (filedata.count() == 0) {
            filedata = data;
        } else {
            filedata += data;
        }
    }
    return QVariant::fromValue(filedata);
}

void BarometerDataLoader::triggerDaemonRecording() {
    m_iface->call("triggerRecording");
}
