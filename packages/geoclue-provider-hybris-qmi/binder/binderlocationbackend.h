/*
    Copyright (c) 2015 - 2020 Jolla Ltd.
    Copyright (c) 2018 Matti Lehtimäki <matti.lehtimaki@gmail.com>
    Copyright (c) 2025 Jolla Mobile Ltd

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef BINDERLOCATIONBACKEND_H
#define BINDERLOCATIONBACKEND_H

#include "hybrislocationbackend.h"

#include <QtCore/QObject>
#include <QtCore/QString>

#include <gbinder.h>
#include <locationsettings.h>

#include "gnss-binder-types.h"
#include "locationtypes.h"

enum HybrisApnIpTypeEnum {
    HYBRIS_APN_IP_INVALID  = 0,
    HYBRIS_APN_IP_IPV4     = 1,
    HYBRIS_APN_IP_IPV6     = 2,
    HYBRIS_APN_IP_IPV4V6   = 3
};

HybrisApnIpType fromContextProtocol(const QString &protocol);

const void *geoclue_binder_gnss_decode_struct1(
    GBinderReader *in,
    guint size);

#define geoclue_binder_gnss_decode_struct(type,in) \
    ((const type*)geoclue_binder_gnss_decode_struct1(in, sizeof(type)))

bool nmeaChecksumValid(const QByteArray &nmea);

void parseRmc(const QByteArray &nmea);

void processNmea(gint64 timestamp, const char *nmeaData);

class BinderLocationBackend : public HybrisLocationBackend
{
    Q_OBJECT
public:
    BinderLocationBackend(QObject *parent = 0);
    ~BinderLocationBackend();

    void dropGnss();

    // Gnss
    virtual bool gnssInit() = 0;
    virtual bool gnssStart() = 0;
    virtual bool gnssStop() = 0;
    virtual void gnssCleanup() = 0;
    virtual bool gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs) = 0;
    virtual bool gnssInjectLocation(int timestamp, double latitudeDegrees, double longitudeDegrees, float accuracyMeters) = 0;
    virtual void gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags) = 0;
    virtual bool gnssSetPositionMode(HybrisGnssPositionMode mode, HybrisGnssPositionRecurrence recurrence,
                                     uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                     uint32_t preferredTimeMs) = 0;

    // GnssDebug
    virtual void gnssDebugInit() = 0;

    // GnnNi
    virtual void gnssNiInit() = 0;
    virtual void gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse) = 0;

    // GnssXtra
    virtual void gnssXtraInit() = 0;
    virtual bool gnssXtraInjectXtraData(QByteArray &xtraData) = 0;

    // AGnss
    virtual void aGnssInit() = 0;
    virtual bool aGnssDataConnClosed() = 0;
    virtual bool aGnssDataConnFailed() = 0;
    virtual bool aGnssDataConnOpen(const QByteArray &apn, const QString &protocol) = 0;
    virtual int aGnssSetServer(HybrisAGnssType type, const char* hostname, int port) = 0;

    // AGnssRil
    virtual void aGnssRilInit() = 0;

protected:
    GBinderRemoteObject *getExtensionObject(GBinderRemoteReply *reply,
                                            bool allowNull = false);
    virtual bool isReplySuccess(GBinderRemoteReply *reply) = 0;

    gulong m_death_id;
    char *m_fqname;
    GBinderServiceManager *m_sm;

    GBinderClient *m_clientGnss;
    GBinderRemoteObject *m_remoteGnss;
    GBinderLocalObject *m_callbackGnss;

    GBinderClient *m_clientGnssDebug;
    GBinderRemoteObject *m_remoteGnssDebug;

    GBinderClient *m_clientGnssNi;
    GBinderRemoteObject *m_remoteGnssNi;
    GBinderLocalObject *m_callbackGnssNi;

    GBinderClient *m_clientGnssXtra;
    GBinderRemoteObject *m_remoteGnssXtra;
    GBinderLocalObject *m_callbackGnssXtra;

    GBinderClient *m_clientAGnss;
    GBinderRemoteObject *m_remoteAGnss;
    GBinderLocalObject *m_callbackAGnss;

    GBinderClient *m_clientAGnssRil;
    GBinderRemoteObject *m_remoteAGnssRil;
    GBinderLocalObject *m_callbackAGnssRil;
};

#endif // BINDERLOCATIONBACKEND_H
