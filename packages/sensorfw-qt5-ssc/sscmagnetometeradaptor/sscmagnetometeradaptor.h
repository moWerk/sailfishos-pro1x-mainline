/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCMAGNETOMETERADAPTOR_H
#define SSCMAGNETOMETERADAPTOR_H

#include "sscadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/magneticfield.h"

class SscMagnetometerAdaptor : public SscAdaptor
{
    Q_OBJECT
public:
    static DeviceAdaptor *factoryMethod(const QString &id) { return new SscMagnetometerAdaptor(id); }
    explicit SscMagnetometerAdaptor(const QString &id);
    ~SscMagnetometerAdaptor();

private:
    static void measurement(GObject *sensor, gfloat x, gfloat y, gfloat z, gpointer data);
    DeviceAdaptorRingBuffer<CalibratedMagneticFieldData> *buffer;
};

#endif
