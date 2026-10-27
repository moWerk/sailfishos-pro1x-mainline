/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCPROXIMITYADAPTOR_H
#define SSCPROXIMITYADAPTOR_H

#include "sscadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/proximity.h"

class SscProximityAdaptor : public SscAdaptor
{
    Q_OBJECT
public:
    static DeviceAdaptor *factoryMethod(const QString &id) { return new SscProximityAdaptor(id); }
    explicit SscProximityAdaptor(const QString &id);
    ~SscProximityAdaptor();

private:
    static void measurement(GObject *sensor, gboolean near, gpointer data);
    DeviceAdaptorRingBuffer<ProximityData> *buffer;
};

#endif
