// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The sensor side of the daemon: enroll, identify, enumerate and remove on
 * the mafp trusted application, each run on a worker thread because every
 * step is a blocking TrustZone call and the finger waits poll the sensor.
 * Results come back as signals; the receiver lives on the main thread, so
 * Qt queues them.
 *
 * Same surface as the AndroidFP class of sailfish-fpd-community, so the
 * D-Bus side could be shared between the two.
 */
#ifndef MAFPBACKEND_H
#define MAFPBACKEND_H

#include "mafpta.h"
#include "sensorpower.h"

#include <QObject>
#include <QList>
#include <QString>

#include <atomic>
#include <mutex>
#include <thread>

class MafpBackend : public QObject
{
    Q_OBJECT
public:
    enum Acquired {
        AcquiredGood,
        AcquiredPartial,
        AcquiredInsufficient,
        AcquiredImagerDirty,
        AcquiredTooSlow,
        AcquiredTooFast,
        AcquiredVendor,
    };

    explicit MafpBackend(QObject *parent = nullptr);
    ~MafpBackend();

    /* Sensor power, TEE session, TA init, template store. Blocking; retried by the caller. */
    bool start();
    bool isReady() const { return m_ta.isStarted(); }
    QString lastError() const { return QString::fromStdString(m_ta.error()); }

    void enroll();
    void remove(uint32_t finger);
    void clear();
    void cancel();
    void authenticate();
    void enumerate();
    QList<uint32_t> fingerprints() const;

private slots:
    void recoverIfDead();

signals:
    void failed(const QString &message);
    void succeeded(uint32_t fingerId);       /* after an enrolment */
    void removed(uint32_t finger);           /* 0 for clear */
    void authenticated(uint32_t fingerId);
    void enrollProgress(float progress);     /* 0..1 */
    void acquired(int info);                 /* Acquired */
    void enumerated();

private:
    bool begin(const char *what);
    void finish();
    void runEnroll();
    void runAuthenticate();
    void runEnumerate();
    void runRemove(uint32_t finger, bool all);
    bool waitTouch(int mode, bool touched, double seconds);
    bool sleepStep();

    MafpTa m_ta;
    SensorPower m_power;
    std::thread m_worker;
    std::mutex m_mutex;              /* one operation at a time, and m_fingers */
    std::atomic<bool> m_busy;
    std::atomic<bool> m_cancel;
    QList<uint32_t> m_fingers;
};

#endif
