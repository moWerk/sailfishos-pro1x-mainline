/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscproximityadaptorplugin.h"
#include "sscproximityadaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SscProximityAdaptorPlugin::Register(class Loader &)
{
    qCInfo(lcSensorFw) << "registering sscproximityadaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SscProximityAdaptor>("proximityadaptor");
}
