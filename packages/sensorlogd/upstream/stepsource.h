/*
 * Copyright (C) 2026 moWerk <mo@mowerk.net>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef STEPSOURCE_H
#define STEPSOURCE_H

/*
 * The step count source. AsteroidOS reads QtSensors' QStepCounterSensor; SailfishOS ships
 * QtSensors 5.6, which has no step counter, so there the count comes from sensorfw's
 * stepcountersensor through its client library (SENSORLOGD_SENSORFW).
 */

#include <QObject>

#ifdef SENSORLOGD_SENSORFW
#include <sensormanagerinterface.h>
#include <stepcountersensor_i.h>

class StepSource : public QObject
{
    Q_OBJECT
public:
    explicit StepSource(QObject *parent = nullptr) : QObject(parent)
    {
        SensorManagerInterface &sm = SensorManagerInterface::instance();
        if (!sm.loadPlugin("stepcountersensor"))
            qWarning() << "sensorfw has no stepcountersensor";
        sm.registerSensorInterface<StepCounterSensorChannelInterface>("stepcountersensor");
        m_iface = StepCounterSensorChannelInterface::interface("stepcountersensor");
        if (!m_iface) {
            qWarning() << "cannot open sensorfw's stepcountersensor";
            return;
        }
        connect(m_iface, &StepCounterSensorChannelInterface::StepCounterChanged,
                this, [this](const Unsigned &value) { m_steps = value.x(); m_active = true; emit stepsChanged(); });
    }
    /* standbyOverride: keep counting while the display is off, which is the point of a logger */
    void start()
    {
        if (!m_iface)
            return;
        m_iface->setStandbyOverride(true);
        m_iface->start();
    }
    bool isActive() const { return m_active; }
    int steps() const { return m_steps; }

signals:
    void stepsChanged();

private:
    StepCounterSensorChannelInterface *m_iface = nullptr;
    int m_steps = 0;
    bool m_active = false;
};
#else
#include <QtSensors/QStepCounterSensor>

class StepSource : public QObject
{
    Q_OBJECT
public:
    explicit StepSource(QObject *parent = nullptr) : QObject(parent), m_sensor(new QStepCounterSensor(this))
    {
        connect(m_sensor, &QStepCounterSensor::readingChanged, this, &StepSource::stepsChanged);
    }
    void start() { m_sensor->start(); }
    bool isActive() const { return m_sensor->isActive(); }
    int steps() const { return m_sensor->reading() ? m_sensor->reading()->steps() : 0; }

signals:
    void stepsChanged();

private:
    QStepCounterSensor *m_sensor;
};
#endif

#endif // STEPSOURCE_H
