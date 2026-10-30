 /*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDebug>
#include <QDir>
#include "common.h"

QString fileNameForDate(QDate date, QString prefix) {
    QSettings settings("asteroid", "sensorlogd"); // this should be moved out of here at some point TODO
    return settings.value("loggerRootPath", QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.asteroid-sensorlogd/").toString() + prefix + "/" + date.toString("yyyy-MM-dd.log");
}

QString sensorDirPath(QString prefix) {
    QSettings settings("asteroid", "sensorlogd"); // this should be moved out of here at some point TODO
    return settings.value("loggerRootPath", QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.asteroid-sensorlogd/").toString() + prefix + "/";
}

void fileAddRecord(QString sensorPrefix, QString logdata, QDateTime recordTime) { //adds a record to today's log file for the given sensor
    qDebug() << fileNameForDate(recordTime.date(), sensorPrefix);
    QFile file(fileNameForDate(recordTime.date(), sensorPrefix));
    if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qDebug() << "failed to open file";
        return;
    }
    file.seek(file.size());
    QTextStream out(&file);
    // recordTime, not now: currentSecsSinceEpoch() is static (and Qt 5.8+), so the end-of-day
    // record of a date change got the current time
    out << QString::number(recordTime.toMSecsSinceEpoch() / 1000) + ":" + logdata + "\n";
    file.close();
}

void setupFilePath(QString sensorPrefix) {
    QSettings settings("asteroid","sensorlogd"); //this should be moved out of here at some point TODO
    QDir::root().mkpath(settings.value("loggerRootPath",QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.asteroid-sensorlogd/").toString() + sensorPrefix);
}
