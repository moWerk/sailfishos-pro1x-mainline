/*
    Copyright (c) 2015 - 2021 Jolla Ltd.
    Copyright (c) 2018 Matti Lehtimäki <matti.lehtimaki@gmail.com>
    Copyright (c) 2025 Jolla Mobile Ltd

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "binderlocationbackend.h"
#include "binderlocationbackend_aidl.h"
#include "binderlocationbackend_hidl.h"

#include "hybrisprovider.h"

#include <strings.h>
#include <sys/time.h>

HybrisLocationBackend *getLocationBackend()
{
    if (BinderLocationBackendAidl::isSupported()) {
        return qobject_cast<HybrisLocationBackend *>(new BinderLocationBackendAidl());
    } else if (BinderLocationBackendHidl::isSupported()) {
        return qobject_cast<HybrisLocationBackend *>(new BinderLocationBackendHidl());
    } else {
        return nullptr;
    }
}

/*==========================================================================*
 * Implementation
 *==========================================================================*/

HybrisApnIpType fromContextProtocol(const QString &protocol)
{
    if (protocol == QLatin1String("ip"))
        return HYBRIS_APN_IP_IPV4;
    else if (protocol == QLatin1String("ipv6"))
        return HYBRIS_APN_IP_IPV6;
    else if (protocol == QLatin1String("dual"))
        return HYBRIS_APN_IP_IPV4V6;
    else
        return HYBRIS_APN_IP_INVALID;
}

const void *geoclue_binder_gnss_decode_struct1(
    GBinderReader *in,
    guint size)
{
    const void *result = nullptr;
    GBinderBuffer *buf = gbinder_reader_read_buffer(in);

    if (buf && buf->size == size) {
        result = buf->data;
    }
    gbinder_buffer_free(buf);
    return result;
}

#define geoclue_binder_gnss_decode_struct(type,in) \
    ((const type*)geoclue_binder_gnss_decode_struct1(in, sizeof(type)))

bool nmeaChecksumValid(const QByteArray &nmea)
{
    unsigned char checksum = 0;
    for (int i = 1; i < nmea.length(); ++i) {
        if (nmea.at(i) == '*') {
            if (nmea.length() < i+3)
                return false;

            checksum ^= nmea.mid(i+1, 2).toInt(0, 16);

            break;
        }

        checksum ^= nmea.at(i);
    }

    return checksum == 0;
}

void parseRmc(const QByteArray &nmea)
{
    QList<QByteArray> fields = nmea.split(',');
    if (fields.count() < 12)
        return;

    bool ok;
    double variation = fields.at(10).toDouble(&ok);
    if (ok) {
        if (fields.at(11) == "W")
            variation = -variation;

        QMetaObject::invokeMethod(staticProvider, "setMagneticVariation", Qt::QueuedConnection,
                                  Q_ARG(double, variation));
    }
}

void processNmea(gint64 timestamp, const char *nmeaData)
{
    int length = strlen(nmeaData);
    while (length > 0 && isspace(nmeaData[length-1]))
        --length;

    if (length == 0)
        return;

    QByteArray nmea = QByteArray::fromRawData(nmeaData, length);

    qCDebug(lcGeoclueHybrisNmea) << timestamp << nmea;

    if (!nmeaChecksumValid(nmea))
        return;

    // truncate checksum and * from end of sentence
    nmea.truncate(nmea.length()-3);

    if (nmea.startsWith("$GPRMC"))
        parseRmc(nmea);
}

/*==========================================================================*
 * Backend class
 *==========================================================================*/

BinderLocationBackend::BinderLocationBackend(QObject *parent)
    : HybrisLocationBackend(parent), m_death_id(0), m_fqname(nullptr), m_sm(nullptr),
      m_clientGnss(nullptr), m_remoteGnss(nullptr), m_callbackGnss(nullptr),
      m_clientGnssDebug(nullptr), m_remoteGnssDebug(nullptr),
      m_clientGnssNi(nullptr), m_remoteGnssNi(nullptr), m_callbackGnssNi(nullptr),
      m_clientGnssXtra(nullptr), m_remoteGnssXtra(nullptr), m_callbackGnssXtra(nullptr),
      m_clientAGnss(nullptr), m_remoteAGnss(nullptr), m_callbackAGnss(nullptr),
      m_clientAGnssRil(nullptr), m_remoteAGnssRil(nullptr), m_callbackAGnssRil(nullptr)
{
}

BinderLocationBackend::~BinderLocationBackend()
{
    dropGnss();
}

void BinderLocationBackend::dropGnss()
{
    if (m_callbackGnss) {
        gbinder_local_object_drop(m_callbackGnss);
        m_callbackGnss = nullptr;
    }
    if (m_clientGnss) {
        gbinder_client_unref(m_clientGnss);
        m_clientGnss = nullptr;
    }
    if (m_remoteGnss) {
        gbinder_remote_object_remove_handler(m_remoteGnss, m_death_id);
        gbinder_remote_object_unref(m_remoteGnss);
        m_death_id = 0;
        m_remoteGnss = nullptr;
    }
    if (m_clientGnssDebug) {
        gbinder_client_unref(m_clientGnssDebug);
        m_clientGnssDebug = nullptr;
    }
    if (m_remoteGnssDebug) {
        gbinder_remote_object_unref(m_remoteGnssDebug);
        m_remoteGnssDebug = nullptr;
    }
    if (m_callbackGnssNi) {
        gbinder_local_object_drop(m_callbackGnssNi);
        m_callbackGnssNi = nullptr;
    }
    if (m_clientGnssNi) {
        gbinder_client_unref(m_clientGnssNi);
        m_clientGnssNi = nullptr;
    }
    if (m_remoteGnssNi) {
        gbinder_remote_object_unref(m_remoteGnssNi);
        m_remoteGnssNi = nullptr;
    }
    if (m_callbackGnssXtra) {
        gbinder_local_object_drop(m_callbackGnssXtra);
        m_callbackGnssXtra = nullptr;
    }
    if (m_clientGnssXtra) {
        gbinder_client_unref(m_clientGnssXtra);
        m_clientGnssXtra = nullptr;
    }
    if (m_remoteGnssXtra) {
        gbinder_remote_object_unref(m_remoteGnssXtra);
        m_remoteGnssXtra = nullptr;
    }
    if (m_callbackAGnss) {
        gbinder_local_object_drop(m_callbackAGnss);
        m_callbackAGnss = nullptr;
    }
    if (m_clientAGnss) {
        gbinder_client_unref(m_clientAGnss);
        m_clientAGnss = nullptr;
    }
    if (m_remoteAGnss) {
        gbinder_remote_object_unref(m_remoteAGnss);
        m_remoteAGnss = nullptr;
    }
    if (m_callbackAGnssRil) {
        gbinder_local_object_drop(m_callbackAGnssRil);
        m_callbackAGnssRil = nullptr;
    }
    if (m_clientAGnssRil) {
        gbinder_client_unref(m_clientAGnssRil);
        m_clientAGnssRil = nullptr;
    }
    if (m_remoteAGnssRil) {
        gbinder_remote_object_unref(m_remoteAGnssRil);
        m_remoteAGnssRil = nullptr;
    }
    if (m_sm) {
        gbinder_servicemanager_unref(m_sm);
        m_sm = nullptr;
    }

    g_free(m_fqname);
    m_fqname = nullptr;
}

GBinderRemoteObject *BinderLocationBackend::getExtensionObject(GBinderRemoteReply *reply, bool allowNull)
{
    GBinderReader reader;
    gint32 status;

    if (!reply) {
        return nullptr;
    }

    gbinder_remote_reply_init_reader(reply, &reader);

    if (!gbinder_reader_read_int32(&reader, &status) || status != 0) {
        if (!allowNull) {
            qWarning("Failed to get extension object %d", status);
        }
        return nullptr;
    }

    return gbinder_reader_read_object(&reader);
}
