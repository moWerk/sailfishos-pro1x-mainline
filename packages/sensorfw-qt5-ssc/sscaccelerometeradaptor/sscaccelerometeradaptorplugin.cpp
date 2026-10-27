/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscaccelerometeradaptorplugin.h"
#include "sscaccelerometeradaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SscAccelerometerAdaptorPlugin::Register(class Loader &)
{
    qCInfo(lcSensorFw) << "registering sscaccelerometeradaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SscAccelerometerAdaptor>("accelerometeradaptor");
}
