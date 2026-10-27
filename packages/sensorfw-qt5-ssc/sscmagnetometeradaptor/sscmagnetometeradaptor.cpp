/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscmagnetometeradaptor.h"
#include "datatypes/utils.h"

#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libssc-sensor-magnetometer.h>
}
#pragma pop_macro("signals")

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298
#define RADIANS_TO_MILLIDEGREES 57295.7795

static const SscOps ops = {
    "mag",
    ssc_sensor_magnetometer_new,
    reinterpret_cast<gpointer (*)(GAsyncResult *, GError **)>(ssc_sensor_magnetometer_new_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_magnetometer_open),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_magnetometer_open_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_magnetometer_close),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_magnetometer_close_finish),
    nullptr,
};

static SscOps opsWith(GCallback cb)
{
    SscOps o = ops;
    o.measurement = cb;
    return o;
}

SscMagnetometerAdaptor::SscMagnetometerAdaptor(const QString &id)
    : SscAdaptor(id, opsWith(G_CALLBACK(SscMagnetometerAdaptor::measurement)))
{
    buffer = new DeviceAdaptorRingBuffer<CalibratedMagneticFieldData>(1);
    setAdaptedSensor("magnetometer", "Internal magnetometer coordinates", buffer);
    setDescription("Qualcomm SSC magnetometer (libssc)");
    introduceAvailableInterval(DataRange(10000, 1000000, 0));
    setDefaultInterval(100000);
}

SscMagnetometerAdaptor::~SscMagnetometerAdaptor()
{
    delete buffer;
}

void SscMagnetometerAdaptor::measurement(GObject *, gfloat x, gfloat y, gfloat z, gpointer data)
{
    SscMagnetometerAdaptor *self = static_cast<SscMagnetometerAdaptor *>(data);
    if (!self->accepting())
        return;
    CalibratedMagneticFieldData *d = self->buffer->nextSlot();
    d->timestamp_ = Utils::getTimeStamp();
    /* libssc: uT; sensorfw: nT. The SSC reports calibrated values. */
    d->x_ = d->rx_ = x * 1000;
    d->y_ = d->ry_ = y * 1000;
    d->z_ = d->rz_ = z * 1000;
    d->level_ = 3;
    self->buffer->commit();
    self->buffer->wakeUpReaders();
}
