/*
 * SscAdaptor: sensorfw device adaptor base for Qualcomm SSC sensors through libssc
 * (sensors behind the ADSP on SM6115 and similar, reached over QRTR; no IIO device).
 * libssc is GLib based; sensord's Qt main loop runs on the GLib default context, so the
 * async callbacks are dispatched in sensord's main thread without a worker thread.
 *
 * Shaped like HybrisAdaptor: per-sensor subclasses own the ring buffer and convert the
 * libssc "measurement" signal into sensorfw data.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#ifndef SSCADAPTOR_H
#define SSCADAPTOR_H

#include "deviceadaptor.h"

/* Qt's "signals" macro breaks GLib headers (struct fields named signals) */
#pragma push_macro("signals")
#undef signals
#include <glib-object.h>
#include <gio/gio.h>
#pragma pop_macro("signals")

struct SscOps {
    const char *name;
    void (*create)(GCancellable *, GAsyncReadyCallback, gpointer);
    gpointer (*createFinish)(GAsyncResult *, GError **);
    void (*open)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer);
    gboolean (*openFinish)(gpointer, GAsyncResult *, GError **);
    void (*close)(gpointer, GCancellable *, GAsyncReadyCallback, gpointer);
    gboolean (*closeFinish)(gpointer, GAsyncResult *, GError **);
    GCallback measurement; /* connected with this adaptor as user data */
};

class SscAdaptor : public DeviceAdaptor
{
    Q_OBJECT
public:
    SscAdaptor(const QString &id, const SscOps &ops);
    ~SscAdaptor();

    void init() override {}
    bool startAdaptor() override { return true; }
    void stopAdaptor() override;
    bool startSensor() override;
    void stopSensor() override;
    bool standby() override;
    bool resume() override;

protected:
    /* Called from the measurement handlers: drop samples while not running. */
    bool accepting() const { return m_isRunning; }

private:
    enum State { Idle, Creating, Closed, Opening, Open, Closing };

    void evaluate();
    void step();
    static void createDone(GObject *, GAsyncResult *, gpointer);
    static void openDone(GObject *, GAsyncResult *, gpointer);
    static void closeDone(GObject *, GAsyncResult *, gpointer);

    SscOps m_ops;
    gpointer m_sensor;
    gulong m_handler;
    GCancellable *m_cancel;
    State m_state;
    bool m_shouldBeRunning;
    bool m_inStandbyMode;
    bool m_isRunning;
};

#endif
