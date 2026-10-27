/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCPROXIMITYADAPTORPLUGIN_H
#define SSCPROXIMITYADAPTORPLUGIN_H

#include "plugin.h"

class SscProximityAdaptorPlugin : public Plugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "com.nokia.SensorService.Plugin/1.0")
private:
    void Register(class Loader &l) override;
};

#endif
