/*
 * Copyright (C) 2023 Arseniy Movshev <dodoradio@outlook.com>
 * This file is part of sensorlogd, a sensor logger for the AsteroidOS smartwatch OS.
 *
 * sensorlogd is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 * sensorlogd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include "sensorlogdqmlplugin.h"
#include "loggerSettings.h"
#include "stepsDataLoader.h"
#include "hrDataLoader.h"
#include "weightDataLoader.h"
#include "barometerDataLoader.h"
#include <QtQml>

LogdPlugin::LogdPlugin(QObject* parent)
    : QQmlExtensionPlugin(parent)
{
}

void LogdPlugin::registerTypes(const char* uri)
{
    Q_ASSERT(uri == QLatin1String("org.asteroid.sensorlogd"));
    qmlRegisterType<StepsDataLoader>(uri, 1, 0, "StepsDataLoader");
    qmlRegisterType<HrDataLoader>(uri, 1, 0, "HrDataLoader");
    qmlRegisterType<WeightDataLoader>(uri, 1, 0, "WeightDataLoader");
    qmlRegisterType<BarometerDataLoader>(uri, 1, 0, "BarometerDataLoader");
    qmlRegisterType<LoggerSettings>(uri, 1, 0, "LoggerSettings");
}
