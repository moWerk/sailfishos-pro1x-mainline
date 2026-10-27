/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscgyroscopeadaptor.h"
#include "datatypes/utils.h"

#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libssc-sensor-gyroscope.h>
}
#pragma pop_macro("signals")

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298
#define RADIANS_TO_MILLIDEGREES 57295.7795

static const SscOps ops = {
    "gyro",
    ssc_sensor_gyroscope_new,
    reinterpret_cast<gpointer (*)(GAsyncResult *, GError **)>(ssc_sensor_gyroscope_new_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_gyroscope_open),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_gyroscope_open_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_gyroscope_close),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_gyroscope_close_finish),
    nullptr,
};

static SscOps opsWith(GCallback cb)
{
    SscOps o = ops;
    o.measurement = cb;
    return o;
}

SscGyroscopeAdaptor::SscGyroscopeAdaptor(const QString &id)
    : SscAdaptor(id, opsWith(G_CALLBACK(SscGyroscopeAdaptor::measurement)))
{
    buffer = new DeviceAdaptorRingBuffer<TimedXyzData>(1);
    setAdaptedSensor("gyroscopeadaptor", "Internal gyroscope coordinates", buffer);
    setDescription("Qualcomm SSC gyroscope (libssc)");
    introduceAvailableInterval(DataRange(10000, 1000000, 0));
    setDefaultInterval(100000);
}

SscGyroscopeAdaptor::~SscGyroscopeAdaptor()
{
    delete buffer;
}

void SscGyroscopeAdaptor::measurement(GObject *, gfloat x, gfloat y, gfloat z, gpointer data)
{
    SscGyroscopeAdaptor *self = static_cast<SscGyroscopeAdaptor *>(data);
    if (!self->accepting())
        return;
    TimedXyzData *d = self->buffer->nextSlot();
    d->timestamp_ = Utils::getTimeStamp();
    /* libssc: rad/s; sensorfw: milli-degrees/s */
    d->x_ = x * RADIANS_TO_MILLIDEGREES;
    d->y_ = y * RADIANS_TO_MILLIDEGREES;
    d->z_ = z * RADIANS_TO_MILLIDEGREES;
    self->buffer->commit();
    self->buffer->wakeUpReaders();
}
