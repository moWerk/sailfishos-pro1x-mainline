/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscgyroscopeadaptorplugin.h"
#include "sscgyroscopeadaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SscGyroscopeAdaptorPlugin::Register(class Loader &)
{
    qCInfo(lcSensorFw) << "registering sscgyroscopeadaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SscGyroscopeAdaptor>("gyroscopeadaptor");
}
