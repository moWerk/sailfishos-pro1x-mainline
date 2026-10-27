/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscmagnetometeradaptorplugin.h"
#include "sscmagnetometeradaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SscMagnetometerAdaptorPlugin::Register(class Loader &)
{
    qCInfo(lcSensorFw) << "registering sscmagnetometeradaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SscMagnetometerAdaptor>("magnetometeradaptor");
}
