// SPDX-License-Identifier: GPL-3.0-or-later
#include "fpd.h"

#include <QDebug>
#include <QDBusConnection>
#include <QCoreApplication>
#include <QDir>
#include <QDataStream>
#include <QFile>

#define FINGERPRINT_PATH "/var/lib/sailfish-fpd-mafp"
#define FINGERPRINT_FILE "fingerprints.db"

Fpd::Fpd()
{
    connect(&m_backend, &MafpBackend::enrollProgress, this, &Fpd::onEnrollProgress);
    connect(&m_backend, &MafpBackend::succeeded, this, &Fpd::onSucceeded);
    connect(&m_backend, &MafpBackend::failed, this, &Fpd::onFailed);
    connect(&m_backend, &MafpBackend::removed, this, &Fpd::onRemoved);
    connect(&m_backend, &MafpBackend::acquired, this, &Fpd::onAcquired);
    connect(&m_backend, &MafpBackend::authenticated, this, &Fpd::onAuthenticated);
    connect(&m_backend, &MafpBackend::enumerated, this, &Fpd::onEnumerated);

    /* Identify gives up after 30 s, as the community daemon does. */
    m_cancelTimer.setSingleShot(true);
    m_cancelTimer.setInterval(30000);
    connect(&m_cancelTimer, &QTimer::timeout, this, &Fpd::onIdentifyTimeout);

    QDir dir(QStringLiteral(FINGERPRINT_PATH));
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        qWarning() << "cannot create" << FINGERPRINT_PATH;
    QFile::setPermissions(dir.absolutePath(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    m_fingerDatabasePath = dir.absoluteFilePath(QStringLiteral(FINGERPRINT_FILE));

    registerDBus();

    /* The TA loader and the supplicant may come up after us: keep trying. */
    m_startTimer.setInterval(10000);
    connect(&m_startTimer, &QTimer::timeout, this, &Fpd::startBackend);
    startBackend();
}

void Fpd::startBackend()
{
    if (m_backend.isReady())
        return;
    if (m_backend.start()) {
        m_startTimer.stop();
        enumerate();
    } else {
        if (!m_startTimer.isActive())
            m_startTimer.start();
    }
}

void Fpd::registerDBus()
{
    if (m_dbusRegistered)
        return;
    QDBusConnection connection = QDBusConnection::systemBus();
    if (!connection.registerService(SERVICE_NAME)) {
        qCritical() << "cannot own" << SERVICE_NAME << connection.lastError().message();
        QCoreApplication::exit(1);
        return;
    }
    if (!connection.registerObject(QStringLiteral("/org/sailfishos/fingerprint1"), this,
                                   QDBusConnection::ExportAllInvokables | QDBusConnection::ExportAllSignals)) {
        qCritical() << "cannot register the object" << connection.lastError().message();
        QCoreApplication::exit(1);
        return;
    }
    m_dbusRegistered = true;
    qInfo() << "registered" << SERVICE_NAME;
}

void Fpd::saveFingers()
{
    QFile f(m_fingerDatabasePath);
    if (!f.open(QIODevice::WriteOnly)) {
        qWarning() << "cannot write" << m_fingerDatabasePath;
        return;
    }
    f.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    QDataStream out(&f);
    out.setVersion(QDataStream::Qt_5_6);
    out << m_fingerMap;
}

/* The name map against what the TA has on file: drop stale names, name unknown templates. */
void Fpd::loadFingers()
{
    QFile f(m_fingerDatabasePath);
    if (f.open(QIODevice::ReadOnly)) {
        QDataStream in(&f);
        in.setVersion(QDataStream::Qt_5_6);
        in >> m_fingerMap;
    } else {
        m_fingerMap.clear();
    }

    QList<uint32_t> onFile = m_backend.fingerprints();
    for (uint32_t k : m_fingerMap.keys()) {
        if (!onFile.contains(k)) {
            qWarning() << "named finger" << k << m_fingerMap.value(k) << "is not on file, forgetting it";
            m_fingerMap.remove(k);
        }
    }
    for (uint32_t k : onFile) {
        if (!m_fingerMap.contains(k)) {
            qWarning() << "unnamed template" << k << "on file";
            m_fingerMap[k] = QStringLiteral("Unknown %1").arg(k);
        }
    }
    saveFingers();
    qInfo() << "fingers:" << m_fingerMap;
}

int Fpd::Enroll(const QString &finger, const QDBusMessage &message)
{
    const QString caller = message.service();
    qInfo() << "Enroll" << finger << "from" << caller;
    if (!m_backend.isReady())
        return FPREPLY_FAILED;
    if (!m_dbusCaller.isEmpty() && m_dbusCaller != caller)
        return FPREPLY_ALREADY_BUSY;
    if (finger.isEmpty())
        return FPREPLY_KEY_IS_INVALID;
    if (m_fingerMap.values().contains(finger))
        return FPREPLY_KEY_ALREADY_EXISTS;
    if (m_state != FPSTATE_IDLE)
        return FPREPLY_ALREADY_BUSY;
    setState(FPSTATE_ENROLLING);
    m_addingFinger = finger;
    m_dbusCaller = caller;
    m_backend.enroll();
    emit EnrollProgressChanged(0);
    return FPREPLY_STARTED;
}

int Fpd::Identify(const QDBusMessage &message)
{
    const QString caller = message.service();
    qInfo() << "Identify from" << caller;
    if (!m_backend.isReady())
        return FPREPLY_FAILED;
    if (!m_dbusCaller.isEmpty() && m_dbusCaller != caller)
        return FPREPLY_ALREADY_BUSY;
    if (m_state != FPSTATE_IDLE)
        return FPREPLY_ALREADY_BUSY;
    if (m_fingerMap.isEmpty())
        return FPREPLY_NO_KEYS_AVAILABLE;
    setState(FPSTATE_IDENTIFYING);
    m_dbusCaller = caller;
    m_cancelTimer.start();
    m_backend.authenticate();
    return FPREPLY_STARTED;
}

void Fpd::Clear(const QDBusMessage &message)
{
    const QString caller = message.service();
    qInfo() << "Clear from" << caller;
    if (!m_backend.isReady())
        return;
    if (!m_dbusCaller.isEmpty() && m_dbusCaller != caller)
        return;
    if (m_state == FPSTATE_IDLE) {
        setState(FPSTATE_REMOVING);
        m_dbusCaller = caller;
        m_backend.clear();
    }
}

QString Fpd::GetState()
{
    return enumName(m_state);
}

QStringList Fpd::GetAll()
{
    return m_fingerMap.values();
}

int Fpd::Abort(const QDBusMessage &message)
{
    const QString caller = message.service();
    qInfo() << "Abort from" << caller;
    if (!m_dbusCaller.isEmpty() && m_dbusCaller != caller)
        return FPREPLY_ALREADY_IDLE;
    if (m_state == FPSTATE_IDLE)
        return FPREPLY_ALREADY_IDLE;
    abort();
    return FPREPLY_STARTED;
}

/* Verify, as the community daemon reads it: a touch of the sensor, reported as Verified. */
int Fpd::Verify(const QDBusMessage &message)
{
    const QString caller = message.service();
    qInfo() << "Verify from" << caller;
    if (!m_backend.isReady())
        return FPREPLY_FAILED;
    if (!m_dbusCaller.isEmpty() && m_dbusCaller != caller)
        return FPREPLY_ALREADY_IDLE;
    if (m_state != FPSTATE_IDLE)
        return FPREPLY_ALREADY_BUSY;
    setState(FPSTATE_VERIFYING);
    m_dbusCaller = caller;
    m_backend.enroll();
    emit EnrollProgressChanged(0);
    return FPREPLY_STARTED;
}

int Fpd::Remove(const QString &finger, const QDBusMessage &message)
{
    const QString caller = message.service();
    qInfo() << "Remove" << finger << "from" << caller;
    if (!m_backend.isReady())
        return FPREPLY_FAILED;
    if (!m_dbusCaller.isEmpty() && m_dbusCaller != caller)
        return FPREPLY_ALREADY_BUSY;
    if (m_state != FPSTATE_IDLE)
        return FPREPLY_ALREADY_BUSY;
    uint32_t key = 0;
    for (auto i = m_fingerMap.constBegin(); i != m_fingerMap.constEnd(); ++i) {
        if (i.value() == finger) {
            key = i.key();
            break;
        }
    }
    if (!key)
        return FPREPLY_KEY_DOES_NOT_EXIST;
    setState(FPSTATE_REMOVING);
    m_dbusCaller = caller;
    m_backend.remove(key);
    return FPREPLY_STARTED;
}

void Fpd::abort()
{
    m_backend.cancel();
    m_cancelTimer.stop();
    emit Aborted();
    setState(FPSTATE_IDLE);
}

void Fpd::enumerate()
{
    if (m_state == FPSTATE_IDLE) {
        setState(FPSTATE_ENUMERATING);
        m_backend.enumerate();
    }
}

void Fpd::onEnrollProgress(float pc)
{
    emit EnrollProgressChanged(static_cast<int>(pc * 100));
    if (m_state == FPSTATE_VERIFYING) {
        emit Verified();
        abort();
    }
}

void Fpd::onSucceeded(uint32_t finger)
{
    qInfo() << "enrolled" << m_addingFinger << "as" << finger;
    m_fingerMap[finger] = m_addingFinger;
    emit Added(m_addingFinger);
    m_addingFinger.clear();
    saveFingers();
    emit ListChanged();
    setState(FPSTATE_IDLE);
}

void Fpd::onFailed(const QString &message)
{
    qInfo() << "backend:" << message;
    emit ErrorInfo(message);
    if (m_state == FPSTATE_IDENTIFYING && message == QLatin1String("FINGER_NOT_RECOGNIZED"))
        return;
    if (m_state != FPSTATE_IDLE) {
        m_cancelTimer.stop();
        emit Failed();
        setState(FPSTATE_IDLE);
    }
}

void Fpd::onAcquired(int info)
{
    AcquiredState s;
    switch (info) {
    case MafpBackend::AcquiredGood: s = FPACQUIRED_GOOD; break;
    case MafpBackend::AcquiredPartial: s = FPACQUIRED_PARTIAL; break;
    case MafpBackend::AcquiredInsufficient: s = FPACQUIRED_INSUFFICIENT; break;
    case MafpBackend::AcquiredImagerDirty: s = FPACQUIRED_IMAGER_DIRTY; break;
    case MafpBackend::AcquiredTooSlow: s = FPACQUIRED_TOO_SLOW; break;
    case MafpBackend::AcquiredTooFast: s = FPACQUIRED_TOO_FAST; break;
    case MafpBackend::AcquiredVendor: s = FPACQUIRED_UNRECOGNIZED; break;
    default: s = FPACQUIRED_GOOD; break;
    }
    /* Every sample is reported: the UI counts them, the community daemon only reports changes. */
    m_acquired = s;
    emit AcquisitionInfo(enumName(m_acquired));
}

void Fpd::onRemoved(uint32_t finger)
{
    if (finger) {
        emit Removed(m_fingerMap.value(finger));
        m_fingerMap.remove(finger);
    } else {
        for (const QString &f : m_fingerMap.values())
            emit Removed(f);
        m_fingerMap.clear();
    }
    saveFingers();
    emit ListChanged();
    setState(FPSTATE_IDLE);
}

void Fpd::onAuthenticated(uint32_t finger)
{
    m_cancelTimer.stop();
    if (m_fingerMap.contains(finger)) {
        emit Identified(m_fingerMap.value(finger));
    } else {
        qWarning() << "matched template" << finger << "has no name";
        emit Identified(QStringLiteral("Unknown %1").arg(finger));
    }
    setState(FPSTATE_IDLE);
}

void Fpd::onIdentifyTimeout()
{
    m_backend.cancel();
    emit Failed();
    setState(FPSTATE_IDLE);
}

void Fpd::onEnumerated()
{
    loadFingers();
    emit ListChanged();
    setState(FPSTATE_IDLE);
}

void Fpd::setState(State newState)
{
    if (newState == FPSTATE_IDLE)
        m_dbusCaller.clear();
    if (newState != m_state) {
        m_state = newState;
        emit StateChanged(enumName(m_state));
    }
}
