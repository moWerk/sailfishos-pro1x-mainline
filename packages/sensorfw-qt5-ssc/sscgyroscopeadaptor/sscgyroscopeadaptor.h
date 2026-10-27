/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCGYROSCOPEADAPTOR_H
#define SSCGYROSCOPEADAPTOR_H

#include "sscadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/xyz.h"

class SscGyroscopeAdaptor : public SscAdaptor
{
    Q_OBJECT
public:
    static DeviceAdaptor *factoryMethod(const QString &id) { return new SscGyroscopeAdaptor(id); }
    explicit SscGyroscopeAdaptor(const QString &id);
    ~SscGyroscopeAdaptor();

private:
    static void measurement(GObject *sensor, gfloat x, gfloat y, gfloat z, gpointer data);
    DeviceAdaptorRingBuffer<TimedXyzData> *buffer;
};

#endif
