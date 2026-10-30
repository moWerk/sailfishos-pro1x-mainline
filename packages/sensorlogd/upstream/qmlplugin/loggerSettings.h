/*/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LOGGERSETTINGS_H
#define LOGGERSETTINGS_H

#include <QDBusInterface>
#include <QObject>
#include <QSettings>

class LoggerSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool heartrateSensorEnabled READ getHeartrateSensorEnabled WRITE setHeartrateSensorEnabled)
    Q_PROPERTY(int heartrateSensorInterval READ getHeartrateSensorInterval WRITE setHeartrateSensorInterval)

    Q_PROPERTY(bool stepCounterEnabled READ getStepCounterEnabled WRITE setStepCounterEnabled)
    Q_PROPERTY(int stepCounterInterval READ getStepCounterInterval WRITE setStepCounterInterval)

    Q_PROPERTY(bool stepGoalEnabled READ getStepGoalEnabled WRITE setStepGoalEnabled)
    Q_PROPERTY(int stepGoalTarget READ getStepGoalTarget WRITE setStepGoalTarget)

public:
    explicit LoggerSettings();

public slots:
    Q_INVOKABLE void reInitLogger();
    bool getHeartrateSensorEnabled();
    void setHeartrateSensorEnabled(bool value);
    int getHeartrateSensorInterval();
    void setHeartrateSensorInterval(int value); // This setting won't change anything live - service needs to be restarted. TODO

    bool getStepCounterEnabled();
    void setStepCounterEnabled(bool value);
    int getStepCounterInterval();
    void setStepCounterInterval(int value); // This setting won't change anything live - service needs to be restarted. TODO

    bool getStepGoalEnabled();
    void setStepGoalEnabled(bool value);
    int getStepGoalTarget();
    void setStepGoalTarget(int value);

private:
    QSettings* m_settings;
    QDBusInterface* m_iface;
};
#endif // LOGGERSETTINGS_H
