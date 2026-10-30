/* SPDX-License-Identifier: LGPL-2.1-only */
#include "sscstepcounteradaptor.h"
#include "datatypes/utils.h"

#pragma push_macro("signals")
#undef signals
extern "C" {
#include <libssc-sensor-pedometer.h>
}
#pragma pop_macro("signals")


static const SscOps ops = {
    "pedometer",
    ssc_sensor_pedometer_new,
    reinterpret_cast<gpointer (*)(GAsyncResult *, GError **)>(ssc_sensor_pedometer_new_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_pedometer_open),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_pedometer_open_finish),
    reinterpret_cast<void (*)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer)>(ssc_sensor_pedometer_close),
    reinterpret_cast<gboolean (*)(gpointer, GAsyncResult *, GError **)>(ssc_sensor_pedometer_close_finish),
    nullptr,
};

static SscOps opsWith(GCallback cb)
{
    SscOps o = ops;
    o.measurement = cb;
    return o;
}

SscStepCounterAdaptor::SscStepCounterAdaptor(const QString &id)
    : SscAdaptor(id, opsWith(G_CALLBACK(SscStepCounterAdaptor::measurement)))
{
    buffer = new DeviceAdaptorRingBuffer<TimedUnsigned>(1);
    setAdaptedSensor("stepcounter", "Steps counted by the Sensor Core since the counter was opened", buffer);
    setDescription("Qualcomm SSC pedometer (libssc)");
    /* on-change: the interval only bounds how often sensord looks */
    introduceAvailableInterval(DataRange(100000, 1000000, 0));
    setDefaultInterval(1000000);
}

SscStepCounterAdaptor::~SscStepCounterAdaptor()
{
    delete buffer;
}

void SscStepCounterAdaptor::measurement(GObject *, guint steps, gpointer data)
{
    SscStepCounterAdaptor *self = static_cast<SscStepCounterAdaptor *>(data);
    if (!self->accepting())
        return;
    TimedUnsigned *d = self->buffer->nextSlot();
    d->timestamp_ = Utils::getTimeStamp();
    /* The Sensor Core counts on the accelerometer by itself, also while the AP sleeps, and
     * reports the running total since this client enabled it. */
    d->value_ = steps;
    self->buffer->commit();
    self->buffer->wakeUpReaders();
}
