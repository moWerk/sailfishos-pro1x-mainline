/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscaccelerometeradaptor.h"
#include "datatypes/utils.h"

#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libssc-sensor-accelerometer.h>
}
#pragma pop_macro("signals")

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298
#define RADIANS_TO_MILLIDEGREES 57295.7795

static const SscOps ops = {
    "accel",
    ssc_sensor_accelerometer_new,
    reinterpret_cast<gpointer (*)(GAsyncResult *, GError **)>(ssc_sensor_accelerometer_new_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_accelerometer_open),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_accelerometer_open_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_accelerometer_close),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_accelerometer_close_finish),
    nullptr,
};

static SscOps opsWith(GCallback cb)
{
    SscOps o = ops;
    o.measurement = cb;
    return o;
}

SscAccelerometerAdaptor::SscAccelerometerAdaptor(const QString &id)
    : SscAdaptor(id, opsWith(G_CALLBACK(SscAccelerometerAdaptor::measurement)))
{
    buffer = new DeviceAdaptorRingBuffer<AccelerationData>(1);
    setAdaptedSensor("accelerometer", "Internal accelerometer coordinates", buffer);
    setDescription("Qualcomm SSC accelerometer (libssc)");
    introduceAvailableInterval(DataRange(10000, 1000000, 0));
    setDefaultInterval(100000);
}

SscAccelerometerAdaptor::~SscAccelerometerAdaptor()
{
    delete buffer;
}

void SscAccelerometerAdaptor::measurement(GObject *, gfloat x, gfloat y, gfloat z, gpointer data)
{
    SscAccelerometerAdaptor *self = static_cast<SscAccelerometerAdaptor *>(data);
    if (!self->accepting())
        return;
    AccelerationData *d = self->buffer->nextSlot();
    d->timestamp_ = Utils::getTimeStamp();
    /* libssc: m/s^2; sensorfw: milli-G */
    d->x_ = x * GRAVITY_RECIPROCAL_THOUSANDS;
    d->y_ = y * GRAVITY_RECIPROCAL_THOUSANDS;
    d->z_ = z * GRAVITY_RECIPROCAL_THOUSANDS;
    self->buffer->commit();
    self->buffer->wakeUpReaders();
}
