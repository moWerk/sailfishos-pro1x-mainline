/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCACCELEROMETERADAPTOR_H
#define SSCACCELEROMETERADAPTOR_H

#include "sscadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/orientationdata.h"

class SscAccelerometerAdaptor : public SscAdaptor
{
    Q_OBJECT
public:
    static DeviceAdaptor *factoryMethod(const QString &id) { return new SscAccelerometerAdaptor(id); }
    explicit SscAccelerometerAdaptor(const QString &id);
    ~SscAccelerometerAdaptor();

private:
    static void measurement(GObject *sensor, gfloat x, gfloat y, gfloat z, gpointer data);
    DeviceAdaptorRingBuffer<AccelerationData> *buffer;
};

#endif
