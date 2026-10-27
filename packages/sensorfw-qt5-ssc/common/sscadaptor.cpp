/*
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#include "sscadaptor.h"
#include "logging.h"

SscAdaptor::SscAdaptor(const QString &id, const SscOps &ops)
    : DeviceAdaptor(id)
    , m_ops(ops)
    , m_sensor(nullptr)
    , m_handler(0)
    , m_cancel(g_cancellable_new())
    , m_state(Idle)
    , m_shouldBeRunning(false)
    , m_inStandbyMode(false)
    , m_isRunning(false)
{
}

SscAdaptor::~SscAdaptor()
{
    g_cancellable_cancel(m_cancel);
    if (m_sensor) {
        if (m_handler)
            g_signal_handler_disconnect(m_sensor, m_handler);
        g_object_unref(m_sensor);
    }
    g_object_unref(m_cancel);
}

void SscAdaptor::stopAdaptor()
{
    m_shouldBeRunning = false;
    evaluate();
}

bool SscAdaptor::startSensor()
{
    if (!m_shouldBeRunning) {
        m_shouldBeRunning = true;
        evaluate();
    }
    return true;
}

void SscAdaptor::stopSensor()
{
    if (m_shouldBeRunning) {
        m_shouldBeRunning = false;
        evaluate();
    }
}

bool SscAdaptor::standby()
{
    if (!m_inStandbyMode) {
        m_inStandbyMode = true;
        evaluate();
    }
    return true;
}

bool SscAdaptor::resume()
{
    if (m_inStandbyMode) {
        m_inStandbyMode = false;
        evaluate();
    }
    return true;
}

/* sensorfw bookkeeping, as HybrisAdaptor::evaluateSensor does it */
void SscAdaptor::evaluate()
{
    AdaptedSensorEntry *entry = getAdaptedSensor();
    if (!entry) {
        qCWarning(lcSensorFw) << id() << "no adapted sensor";
        return;
    }

    bool run = m_shouldBeRunning && (deviceStandbyOverride() || !m_inStandbyMode);
    if (m_isRunning != run) {
        if ((m_isRunning = run)) {
            if (entry->addReference() == 1)
                entry->setIsRunning(true);
        } else {
            if (entry->removeReference() == 0)
                entry->setIsRunning(false);
        }
        qCInfo(lcSensorFw) << id() << m_ops.name << (m_isRunning ? "start" : "stop");
    }
    step();
}

/* Drive the libssc sensor towards the wanted state; one async operation at a time. */
void SscAdaptor::step()
{
    switch (m_state) {
    case Idle:
        if (m_isRunning) {
            m_state = Creating;
            m_ops.create(m_cancel, createDone, this);
        }
        break;
    case Closed:
        if (m_isRunning) {
            m_state = Opening;
            m_ops.open(m_sensor, m_cancel, openDone, this);
        }
        break;
    case Open:
        if (!m_isRunning) {
            m_state = Closing;
            m_ops.close(m_sensor, m_cancel, closeDone, this);
        }
        break;
    case Creating:
    case Opening:
    case Closing:
        break; /* the completion calls step() again */
    }
}

void SscAdaptor::createDone(GObject *, GAsyncResult *res, gpointer data)
{
    SscAdaptor *self = static_cast<SscAdaptor *>(data);
    GError *error = nullptr;
    gpointer sensor = self->m_ops.createFinish(res, &error);

    if (!sensor) {
        qCWarning(lcSensorFw) << self->id() << self->m_ops.name << "SSC sensor not available:"
                              << (error ? error->message : "unknown error");
        g_clear_error(&error);
        self->m_state = Idle;
        return; /* retried on the next start */
    }
    self->m_sensor = sensor;
    self->m_handler = g_signal_connect(sensor, "measurement", self->m_ops.measurement, self);
    self->m_state = Closed;
    self->step();
}

void SscAdaptor::openDone(GObject *, GAsyncResult *res, gpointer data)
{
    SscAdaptor *self = static_cast<SscAdaptor *>(data);
    GError *error = nullptr;

    if (!self->m_ops.openFinish(self->m_sensor, res, &error)) {
        qCWarning(lcSensorFw) << self->id() << self->m_ops.name << "open failed:"
                              << (error ? error->message : "unknown error");
        g_clear_error(&error);
        self->m_state = Closed;
        return;
    }
    self->m_state = Open;
    self->step();
}

void SscAdaptor::closeDone(GObject *, GAsyncResult *res, gpointer data)
{
    SscAdaptor *self = static_cast<SscAdaptor *>(data);
    GError *error = nullptr;

    if (!self->m_ops.closeFinish(self->m_sensor, res, &error)) {
        qCWarning(lcSensorFw) << self->id() << self->m_ops.name << "close failed:"
                              << (error ? error->message : "unknown error");
        g_clear_error(&error);
    }
    self->m_state = Closed;
    self->step();
}
