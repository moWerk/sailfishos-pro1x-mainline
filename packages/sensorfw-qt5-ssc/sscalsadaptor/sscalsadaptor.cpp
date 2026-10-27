/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscalsadaptor.h"
#include "datatypes/utils.h"

#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libssc-sensor-light.h>
}
#pragma pop_macro("signals")

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298
#define RADIANS_TO_MILLIDEGREES 57295.7795

static const SscOps ops = {
    "ambient_light",
    ssc_sensor_light_new,
    reinterpret_cast<gpointer (*)(GAsyncResult *, GError **)>(ssc_sensor_light_new_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_light_open),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_light_open_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_light_close),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_light_close_finish),
    nullptr,
};

static SscOps opsWith(GCallback cb)
{
    SscOps o = ops;
    o.measurement = cb;
    return o;
}

SscAlsAdaptor::SscAlsAdaptor(const QString &id)
    : SscAdaptor(id, opsWith(G_CALLBACK(SscAlsAdaptor::measurement)))
{
    buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
    setAdaptedSensor("als", "Internal ambient light sensor lux values", buffer);
    setDescription("Qualcomm SSC als (libssc)");
    introduceAvailableInterval(DataRange(10000, 1000000, 0));
    setDefaultInterval(100000);
}

SscAlsAdaptor::~SscAlsAdaptor()
{
    delete buffer;
}

void SscAlsAdaptor::measurement(GObject *, gfloat lux, gpointer data)
{
    SscAlsAdaptor *self = static_cast<SscAlsAdaptor *>(data);
    if (!self->accepting())
        return;
    TimedUnsigned *d = self->buffer->nextSlot();
    d->timestamp_ = Utils::getTimeStamp();
    d->value_ = lux < 0 ? 0 : (unsigned)(lux + 0.5f);
    self->buffer->commit();
    self->buffer->wakeUpReaders();
}
