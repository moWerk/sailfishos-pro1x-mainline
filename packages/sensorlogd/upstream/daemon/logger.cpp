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
#include <QDBusInterface>
#include <QDBusConnection>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <QDebug>
#include <QStandardPaths>

#include "logger.h"
#include "../common.h"

#include "sensorPlugins/stepCounter.h"

Logger::Logger(QObject *parent)  :
    QObject(parent){
    if(!QDBusConnection::sessionBus().registerService("org.asteroid.sensorlogd.logger")) qDebug() << "failed to register service";
    if(!QDBusConnection::sessionBus().registerObject("/org/asteroid/sensorlogd/logger", this, QDBusConnection::ExportAllContents)) qDebug() << "failed to register object";
    this->setup();
}

void Logger::setup() {
    m_iface = new QDBusInterface("com.nokia.mce","/com/nokia/mce/signal", "com.nokia.mce.signal", QDBusConnection::systemBus());
    settings = new QSettings;
    daemonFresh = getDaemonFresh();

    heartrateSensorEnabled = this->settings->value("heartrateSensor/enabled",true).toBool();
    stepCounterEnabled = this->settings->value("stepCounter/enabled",true).toBool();
    barometerEnabled = this->settings->value("barometer/enabled",true).toBool();

//intialise HRM
#ifdef SENSORLOGD_HEARTRATE
    if (heartrateSensorEnabled) {
        m_heartrateSensor = new HeartrateSensorPlugin(this,settings->value("heartrateSensor/interval",600000).toInt());
    }
#endif

//initialise step counter
    if (stepCounterEnabled) {
        m_stepCounter = new StepCounterPlugin(this,settings->value("stepCounter/interval",600000).toInt(),daemonFresh);
    }

//initialise barometer
#ifdef SENSORLOGD_BAROMETER
    if (barometerEnabled) {
        m_barometerSensor = new BarometerSensorPlugin(this,settings->value("barometer/interval",600000).toInt());
    }
#endif

    if(!m_iface->isValid()) {
	    qDebug() << "interface is not valid";
	    qDebug() << m_iface->lastError();
    }
    if(connect(m_iface, SIGNAL(display_status_ind(QString)), this, SLOT(displayOn(QString)))) { //this fires when the display turns on
	    qDebug() << "connected display_status signal to slot";
    }
    qDebug() << "sensorlogd setup complete";
    this->triggerRecording();
}

void Logger::resetup() {
    this->setup();
}

void Logger::displayOn(QString displayState) {
    if (displayState == "off")
        return;
    qDebug() << "display on detected";
    uint currTime = QDateTime::currentMSecsSinceEpoch();

#ifdef SENSORLOGD_HEARTRATE
    if (heartrateSensorEnabled) {
        m_heartrateSensor->timeUpdate();
    }
#endif

    if (stepCounterEnabled) {
        m_stepCounter->timeUpdate();
    }

#ifdef SENSORLOGD_BAROMETER
    if (barometerEnabled) {
        m_barometerSensor->timeUpdate();
    }
#endif
}

void Logger::triggerRecording() {
#ifdef SENSORLOGD_HEARTRATE
    if (heartrateSensorEnabled) {
        m_heartrateSensor->triggerRecording();
    }
#endif

    if (stepCounterEnabled) {
        m_stepCounter->triggerRecording();
    }

#ifdef SENSORLOGD_BAROMETER
    if (barometerEnabled) {
        m_barometerSensor->triggerRecording();
    }
#endif
}

bool Logger::getDaemonFresh() {
    QFile file("/proc/sys/kernel/random/boot_id");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "failed to open bootId";
        return 0;
    }
    QTextStream inStream(&file);
    QString currBootId = inStream.readAll().trimmed();
    file.close();
    QSettings settings("asteroid","sensorlogd");
    if(currBootId == settings.value("lastBootId","")) {
        qDebug() << "daemon is not starting for first time this boot";
        return false;
    } else {
        settings.setValue("lastBootId",currBootId);
        qDebug() << "daemon is starting for first time this boot";
        return true;
    }
}

bool dayFileExists(QString sensorPrefix, QDateTime dateTime) {
    return QFile::exists(fileNameForDate(dateTime.date(), sensorPrefix));
}

QStringList fileGetPrevRecord(QString sensorPrefix, QDateTime recordTime) {
    qDebug() << fileNameForDate(recordTime.date(), sensorPrefix);
    QFile file(fileNameForDate(recordTime.date(), sensorPrefix));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "failed to open file";
        return {0,0};
    }
    QTextStream inStream(&file);
    QString line;
    int i;
    while(!inStream.atEnd())
    {
        line = inStream.readLine();
        qDebug() << line;
        i++;
    }
    file.close();
    return line.split(":");
}



