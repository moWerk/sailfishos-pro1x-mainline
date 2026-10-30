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

#include "loggerSettings.h"

LoggerSettings::LoggerSettings()
    : QObject()
{
    m_settings = new QSettings("asteroid", "sensorlogd");
    m_iface = new QDBusInterface("org.asteroid.sensorlogd.logger", "/org/asteroid/sensorlogd/logger", "", QDBusConnection::sessionBus(), this);
    if (!m_iface->isValid()) {
        qDebug() << "interface is not valid";
    } else {
        qDebug() << "interface is valid";
    }
}

void LoggerSettings::reInitLogger()
{
    m_iface->call("resetup");
}

bool LoggerSettings::getHeartrateSensorEnabled()
{
    return this->m_settings->value("heartrateSensor/enabled", true).toBool();
}

void LoggerSettings::setHeartrateSensorEnabled(bool value)
{
    this->m_settings->setValue("heartrateSensor/enabled", value);
}

int LoggerSettings::getHeartrateSensorInterval()
{
    return this->m_settings->value("heartrateSensor/interval", 600000).toInt();
}

void LoggerSettings::setHeartrateSensorInterval(int value)
{
    this->m_settings->setValue("heartrateSensor/interval", value);
}

bool LoggerSettings::getStepCounterEnabled()
{
    return this->m_settings->value("stepCounter/enabled", true).toBool();
}

void LoggerSettings::setStepCounterEnabled(bool value)
{
    this->m_settings->setValue("stepCounter/enabled", value);
}

int LoggerSettings::getStepCounterInterval()
{
    return this->m_settings->value("stepCounter/interval", 600000).toInt();
}

void LoggerSettings::setStepCounterInterval(int value)
{
    this->m_settings->setValue("stepCounter/interval", value);
}

bool LoggerSettings::getStepGoalEnabled()
{
    return this->m_settings->value("stepCounter/goal0/enabled", false).toBool();
}
void LoggerSettings::setStepGoalEnabled(bool value)
{
    this->m_settings->setValue("stepCounter/goal0/enabled", value);
}
int LoggerSettings::getStepGoalTarget()
{
    return this->m_settings->value("stepCounter/goal0/target", 0).toInt();
}

void LoggerSettings::setStepGoalTarget(int value)
{
    this->m_settings->setValue("stepCounter/goal0/target", value);
}
