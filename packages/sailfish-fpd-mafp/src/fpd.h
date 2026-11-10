// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The org.sailfishos.fingerprint1 D-Bus service, as sailfish-devicelock-fpd
 * and the Settings fingerprint pages expect it.
 *
 * Adapted from sailfish-fpd-community (sailfishos-open, GPL-3.0), whose
 * authors reverse engineered the API from Jolla's sailfish-fpd and mce:
 * states, acquired codes, errors and reply codes are theirs. The Android
 * backend is replaced by MafpBackend.
 */
#ifndef FPD_H
#define FPD_H

#include "mafpbackend.h"

#include <QObject>
#include <QMetaEnum>
#include <QTimer>
#include <QDBusMessage>
#include <QMap>
#include <QStringList>

#define SERVICE_NAME "org.sailfishos.fingerprint1"

template<typename QEnum>
QString enumName(const QEnum value)
{
    return QMetaEnum::fromType<QEnum>().valueToKey(value);
}

class Fpd : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", SERVICE_NAME)

public:
    enum State {
        FPSTATE_UNKNOWN,
        FPSTATE_UNSET,
        FPSTATE_IDLE,
        FPSTATE_ENROLLING,
        FPSTATE_IDENTIFYING,
        FPSTATE_ENUMERATING,
        FPSTATE_REMOVING,
        FPSTATE_VERIFYING,
        FPSTATE_ABORTING,
        FPSTATE_TERMINATING
    };
    Q_ENUM(State)

    enum AcquiredState {
        FPACQUIRED_UNKNOWN,
        FPACQUIRED_GOOD,
        FPACQUIRED_PARTIAL,
        FPACQUIRED_INSUFFICIENT,
        FPACQUIRED_IMAGER_DIRTY,
        FPACQUIRED_TOO_SLOW,
        FPACQUIRED_TOO_FAST,
        FPACQUIRED_UNSET,
        FPACQUIRED_UNSPECIFIED,
        FPACQUIRED_UNRECOGNIZED,
    };
    Q_ENUM(AcquiredState)

    enum Reply {
        FPREPLY_STARTED = 0,
        FPREPLY_FAILED = 1,
        FPREPLY_ALREADY_IDLE = 2,
        FPREPLY_ALREADY_BUSY = 3,
        FPREPLY_DENIED = 4,
        FPREPLY_KEY_ALREADY_EXISTS = 5,
        FPREPLY_KEY_DOES_NOT_EXIST = 6,
        FPREPLY_NO_KEYS_AVAILABLE = 7,
        FPREPLY_KEY_IS_INVALID = 8,
    };
    Q_ENUM(Reply)

    Fpd();

    /* org.sailfishos.fingerprint1, API version 1 */
    Q_INVOKABLE int Enroll(const QString &finger, const QDBusMessage &message);
    Q_INVOKABLE int Identify(const QDBusMessage &message);
    Q_INVOKABLE QString GetState();
    Q_INVOKABLE QStringList GetAll();
    Q_INVOKABLE int Abort(const QDBusMessage &message);
    Q_INVOKABLE int Verify(const QDBusMessage &message);
    Q_INVOKABLE int Remove(const QString &finger, const QDBusMessage &message);
    Q_INVOKABLE void Clear(const QDBusMessage &message);

    Q_SIGNAL void Added(const QString &finger);
    Q_SIGNAL void Removed(const QString &finger);
    Q_SIGNAL void Identified(const QString &finger);
    Q_SIGNAL void Aborted();
    Q_SIGNAL void Failed();
    Q_SIGNAL void Verified();
    Q_SIGNAL void StateChanged(const QString &state);
    Q_SIGNAL void EnrollProgressChanged(int progress);
    Q_SIGNAL void AcquisitionInfo(const QString &info);
    Q_SIGNAL void ErrorInfo(const QString &error);
    Q_SIGNAL void ListChanged();

private slots:
    void onEnrollProgress(float pc);
    void onSucceeded(uint32_t finger);
    void onFailed(const QString &message);
    void onAcquired(int info);
    void onRemoved(uint32_t finger);
    void onAuthenticated(uint32_t finger);
    void onIdentifyTimeout();
    void onEnumerated();
    void startBackend();

private:
    void abort();
    void enumerate();
    void setState(State newState);
    void registerDBus();
    void saveFingers();
    void loadFingers();

    MafpBackend m_backend;
    QTimer m_cancelTimer;
    QTimer m_startTimer;
    bool m_dbusRegistered = false;
    QString m_dbusCaller;
    QString m_fingerDatabasePath;
    QMap<uint32_t, QString> m_fingerMap;
    State m_state = FPSTATE_IDLE;
    AcquiredState m_acquired = FPACQUIRED_UNSPECIFIED;
    QString m_addingFinger;
};

#endif
