/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscstepcounteradaptorplugin.h"
#include "sscstepcounteradaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SscStepCounterAdaptorPlugin::Register(class Loader &)
{
    qCInfo(lcSensorFw) << "registering sscstepcounteradaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SscStepCounterAdaptor>("stepcounteradaptor");
}
