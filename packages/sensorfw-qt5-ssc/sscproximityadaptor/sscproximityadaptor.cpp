/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscproximityadaptor.h"
#include "datatypes/utils.h"

#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libssc-sensor-proximity.h>
}
#pragma pop_macro("signals")

#define GRAVITY_RECIPROCAL_THOUSANDS 101.971621298
#define RADIANS_TO_MILLIDEGREES 57295.7795

static const SscOps ops = {
    "proximity",
    ssc_sensor_proximity_new,
    reinterpret_cast<gpointer (*)(GAsyncResult *, GError **)>(ssc_sensor_proximity_new_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_proximity_open),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_proximity_open_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_proximity_close),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_proximity_close_finish),
    nullptr,
};

static SscOps opsWith(GCallback cb)
{
    SscOps o = ops;
    o.measurement = cb;
    return o;
}

SscProximityAdaptor::SscProximityAdaptor(const QString &id)
    : SscAdaptor(id, opsWith(G_CALLBACK(SscProximityAdaptor::measurement)))
{
    buffer = new DeviceAdaptorRingBuffer<ProximityData>(1);
    setAdaptedSensor("proximity", "Internal proximity coordinates", buffer);
    setDescription("Qualcomm SSC proximity (libssc)");
    introduceAvailableInterval(DataRange(10000, 1000000, 0));
    setDefaultInterval(100000);
}

SscProximityAdaptor::~SscProximityAdaptor()
{
    delete buffer;
}

void SscProximityAdaptor::measurement(GObject *, gboolean near, gpointer data)
{
    SscProximityAdaptor *self = static_cast<SscProximityAdaptor *>(data);
    if (!self->accepting())
        return;
    ProximityData *d = self->buffer->nextSlot();
    d->timestamp_ = Utils::getTimeStamp();
    /* libssc reports near/far only */
    d->withinProximity_ = near;
    d->value_ = near ? 0 : 1;
    self->buffer->commit();
    self->buffer->wakeUpReaders();
}
