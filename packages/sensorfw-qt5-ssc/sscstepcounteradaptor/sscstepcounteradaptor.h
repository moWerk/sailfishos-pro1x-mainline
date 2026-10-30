/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCSTEPCOUNTERADAPTOR_H
#define SSCSTEPCOUNTERADAPTOR_H

#include "sscadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/orientationdata.h"

class SscStepCounterAdaptor : public SscAdaptor
{
    Q_OBJECT
public:
    static DeviceAdaptor *factoryMethod(const QString &id) { return new SscStepCounterAdaptor(id); }
    explicit SscStepCounterAdaptor(const QString &id);
    ~SscStepCounterAdaptor();

private:
    static void measurement(GObject *sensor, guint steps, gpointer data);
    DeviceAdaptorRingBuffer<TimedUnsigned> *buffer;
};

#endif
