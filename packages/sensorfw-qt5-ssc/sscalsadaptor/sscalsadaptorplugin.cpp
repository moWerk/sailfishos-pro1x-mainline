/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscalsadaptorplugin.h"
#include "sscalsadaptor.h"
#include "sensormanager.h"
#include "logging.h"

void SscAlsAdaptorPlugin::Register(class Loader &)
{
    qCInfo(lcSensorFw) << "registering sscalsadaptor";
    SensorManager &sm = SensorManager::instance();
    sm.registerDeviceAdaptor<SscAlsAdaptor>("alsadaptor");
}
