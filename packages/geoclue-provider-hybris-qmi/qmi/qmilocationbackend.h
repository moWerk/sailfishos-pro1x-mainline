/*
    Copyright (c) 2026 moWerk

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef QMILOCATIONBACKEND_H
#define QMILOCATIONBACKEND_H

#include "hybrislocationbackend.h"

#include <QtCore/QByteArray>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>

class QSocketNotifier;

// Location backend for mainline kernels: talks to the modem's QMI LOC service
// over a QRTR socket, without an Android location HAL.
class QmiLocationBackend : public HybrisLocationBackend
{
    Q_OBJECT
public:
    QmiLocationBackend(QObject *parent = 0);
    ~QmiLocationBackend();

    // Gnss
    bool gnssInit();
    bool gnssStart();
    bool gnssStop();
    void gnssCleanup();
    bool gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs);
    bool gnssInjectLocation(int timestamp, double latitudeDegrees, double longitudeDegrees, float accuracyMeters);
    void gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags);
    bool gnssSetPositionMode(HybrisGnssPositionMode mode, HybrisGnssPositionRecurrence recurrence,
                             uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                             uint32_t preferredTimeMs);

    // GnssDebug
    void gnssDebugInit();

    // GnnNi
    void gnssNiInit();
    void gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse);

    // GnssXtra
    void gnssXtraInit();
    bool gnssXtraInjectXtraData(QByteArray &xtraData);

    // AGnss
    void aGnssInit();
    bool aGnssDataConnClosed();
    bool aGnssDataConnFailed();
    bool aGnssDataConnOpen(const QByteArray &apn, const QString &protocol);
    int aGnssSetServer(HybrisAGnssType type, const char *hostname, int port);

    // AGnssRil
    void aGnssRilInit();

private slots:
    void readSocket();

private:
    typedef QHash<quint8, QByteArray> Tlvs;

    bool openSocket();
    void closeSocket();
    void lookupService();
    bool sendRequest(quint16 messageId, const QByteArray &tlvs);
    void serviceArrived(quint32 node, quint32 port);
    void serviceLeft();
    void registerAndStart();
    void handleControl(const QByteArray &packet);
    void handleQmi(const QByteArray &packet);
    void handlePosition(const Tlvs &tlvs);
    void handleSatellites(const Tlvs &tlvs);
    bool sendXtraPart();
    void handleXtraInjected(const Tlvs &tlvs);
    void handleXtraValidity(const Tlvs &tlvs);

    int m_fd;
    QSocketNotifier *m_notifier;
    quint32 m_localNode;
    quint32 m_node;
    quint32 m_port;
    bool m_haveService;
    bool m_sessionWanted;
    quint16 m_transaction;
    quint32 m_minIntervalMs;
    QList<int> m_usedPrns;
    QByteArray m_xtraData;
    int m_xtraPart;
};

#endif // QMILOCATIONBACKEND_H
