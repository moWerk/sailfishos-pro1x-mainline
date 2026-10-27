/* SPDX-License-Identifier: LGPL-2.1-only */
#ifndef SSCALSADAPTOR_H
#define SSCALSADAPTOR_H

#include "sscadaptor.h"
#include "deviceadaptorringbuffer.h"
#include "datatypes/timedunsigned.h"

class SscAlsAdaptor : public SscAdaptor
{
    Q_OBJECT
public:
    static DeviceAdaptor *factoryMethod(const QString &id) { return new SscAlsAdaptor(id); }
    explicit SscAlsAdaptor(const QString &id);
    ~SscAlsAdaptor();

private:
    static void measurement(GObject *sensor, gfloat lux, gpointer data);
    DeviceAdaptorRingBuffer<TimedUnsigned> *buffer;
};

#endif
