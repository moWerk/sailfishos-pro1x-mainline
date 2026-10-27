/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCMAGNETOMETERADAPTORPLUGIN_H
#define SSCMAGNETOMETERADAPTORPLUGIN_H

#include "plugin.h"

class SscMagnetometerAdaptorPlugin : public Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.nokia.SensorService.Plugin/1.0")
private:
    void Register(class Loader &l) override;
};

#endif
