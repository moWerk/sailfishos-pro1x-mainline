/*
    Copyright (c) 2026 moWerk

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "qmilocationbackend.h"

#include <QtCore/QDateTime>
#include <QtCore/QLoggingCategory>
#include <QtCore/QSocketNotifier>
#include <QtCore/QtEndian>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<linux/qrtr.h>)
#include <linux/qrtr.h>
#define HAVE_LINUX_QRTR_H
#endif
#endif

#ifndef HAVE_LINUX_QRTR_H
#define AF_QIPCRTR 42
#define QRTR_PORT_CTRL 0xfffffffeu
struct sockaddr_qrtr {
    unsigned short sq_family;
    quint32 sq_node;
    quint32 sq_port;
};
#endif

#include "hybrisprovider.h"

HybrisLocationBackend *getLocationBackend()
{
    return qobject_cast<HybrisLocationBackend *>(new QmiLocationBackend());
}

namespace
{

const double MpsToKnots = 1.943844;

// QRTR name service
const quint32 QrtrTypeNewServer = 4;
const quint32 QrtrTypeDelServer = 5;
const quint32 QrtrTypeNewLookup = 10;

// QMI LOC (service 16), message and TLV ids as in libqmi's qmi-service-loc.json
const quint32 LocService = 16;
const quint8 QmiRequest = 0;
const quint8 QmiResponse = 2;
const quint8 QmiIndication = 4;

const quint16 LocRegisterEvents = 0x0021;
const quint16 LocStart = 0x0022;
const quint16 LocStop = 0x0023;
const quint16 LocPositionReport = 0x0024;
const quint16 LocGnssSvInfo = 0x0025;
const quint16 LocInjectTimeRequest = 0x0028;
const quint16 LocInjectPredictedOrbitsRequest = 0x0029;
const quint16 LocEngineState = 0x002B;
const quint16 LocInjectUtcTime = 0x0038;
const quint16 LocInjectPosition = 0x0039;
const quint16 LocGetPredictedOrbitsValidity = 0x0037;
const quint16 LocInjectXtraData = 0x00A7;

const quint64 EventPositionReport = 1 << 0;
const quint64 EventGnssSatelliteInfo = 1 << 1;
const quint64 EventInjectTimeRequest = 1 << 4;
const quint64 EventInjectPredictedOrbitsRequest = 1 << 5;
const quint64 EventEngineState = 1 << 7;
const quint64 EventFixSessionState = 1 << 8;

const quint8 SessionId = 1;
const quint32 RecurrencePeriodic = 1;
const quint32 IntermediateReportsOff = 2;
const quint32 SessionStatusSuccess = 0;
const quint32 EngineStateOn = 1;
const quint32 EngineStateOff = 2;
const quint32 SvValidSatelliteId = 1 << 1;
const quint32 SvValidElevation = 1 << 5;
const quint32 SvValidAzimuth = 1 << 6;
const quint32 SvValidSnr = 1 << 7;
const int SvEntrySize = 28;

// below this the modem may distrust injected data (ModemManager uses the same floors)
const quint32 MinTimeUncertaintyMs = 1000;
const float MinPositionUncertaintyM = 1000;
// INJECT XTRA DATA takes the file in parts of at most 1 KiB, one at a time
const int XtraPartSize = 1024;
// download a new file when the modem's has less than this left (files last 168 h)
const qint64 XtraRefreshSeconds = 24 * 3600;

QByteArray tlv(quint8 type, const QByteArray &value)
{
    QByteArray out(3, Qt::Uninitialized);
    out[0] = char(type);
    qToLittleEndian<quint16>(quint16(value.size()), reinterpret_cast<uchar *>(out.data() + 1));
    return out + value;
}

template <typename T> QByteArray le(T value)
{
    QByteArray out(sizeof(T), Qt::Uninitialized);
    qToLittleEndian<T>(value, reinterpret_cast<uchar *>(out.data()));
    return out;
}

QByteArray leDouble(double value)
{
    quint64 bits;
    memcpy(&bits, &value, sizeof(bits));
    return le<quint64>(bits);
}

QByteArray leFloat(float value)
{
    quint32 bits;
    memcpy(&bits, &value, sizeof(bits));
    return le<quint32>(bits);
}

template <typename T> bool readLe(const QByteArray &data, int offset, T *value)
{
    if (offset < 0 || offset + int(sizeof(T)) > data.size())
        return false;
    *value = qFromLittleEndian<T>(reinterpret_cast<const uchar *>(data.constData() + offset));
    return true;
}

bool readDouble(const QByteArray &data, double *value)
{
    quint64 bits;
    if (data.size() != 8 || !readLe(data, 0, &bits))
        return false;
    memcpy(value, &bits, sizeof(bits));
    return true;
}

bool readFloat(const QByteArray &data, int offset, float *value)
{
    quint32 bits;
    if (!readLe(data, offset, &bits))
        return false;
    memcpy(value, &bits, sizeof(bits));
    return true;
}

qint64 monotonicMs()
{
    timespec ticks;
    clock_gettime(CLOCK_MONOTONIC, &ticks);
    return qint64(ticks.tv_sec) * 1000 + ticks.tv_nsec / 1000000;
}

}

QmiLocationBackend::QmiLocationBackend(QObject *parent)
    : HybrisLocationBackend(parent), m_fd(-1), m_notifier(nullptr), m_localNode(0), m_node(0),
      m_port(0), m_haveService(false), m_sessionWanted(false), m_transaction(0),
      m_minIntervalMs(1000), m_xtraPart(0)
{
}

QmiLocationBackend::~QmiLocationBackend()
{
    gnssCleanup();
}

bool QmiLocationBackend::gnssInit()
{
    if (!openSocket())
        return false;
    // the service may come later (modem still booting): the lookup reports it when it does
    lookupService();
    return true;
}

bool QmiLocationBackend::gnssStart()
{
    m_sessionWanted = true;
    if (!m_haveService) {
        qCDebug(lcGeoclueHybris) << "QMI LOC service not there yet, starting when it arrives";
        return true;
    }
    registerAndStart();
    return true;
}

bool QmiLocationBackend::gnssStop()
{
    m_sessionWanted = false;
    if (!m_haveService)
        return true;
    // a session left running keeps waking the host every second
    return sendRequest(LocStop, tlv(0x01, le<quint8>(SessionId)));
}

void QmiLocationBackend::gnssCleanup()
{
    if (m_sessionWanted)
        gnssStop();
    closeSocket();
}

bool QmiLocationBackend::gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs,
                                        int32_t uncertaintyMs)
{
    if (!m_haveService)
        return false;
    // the time was valid at timeReferenceMs on the monotonic clock
    const quint64 now = quint64(timeMs + (monotonicMs() - timeReferenceMs));
    const quint32 uncertainty = qMax<quint32>(quint32(qMax(uncertaintyMs, 0)), MinTimeUncertaintyMs);
    qCDebug(lcGeoclueHybris) << "QMI LOC inject UTC time, uncertainty" << uncertainty << "ms";
    return sendRequest(LocInjectUtcTime,
                       tlv(0x01, le<quint64>(now)) + tlv(0x02, le<quint32>(uncertainty)));
}

bool QmiLocationBackend::gnssInjectLocation(int timestamp, double latitudeDegrees,
                                            double longitudeDegrees, float accuracyMeters)
{
    if (!m_haveService)
        return false;
    const float uncertainty = qMax(accuracyMeters, MinPositionUncertaintyM);
    qCDebug(lcGeoclueHybris) << "QMI LOC inject position, uncertainty" << uncertainty << "m";
    return sendRequest(LocInjectPosition,
                       tlv(0x10, leDouble(latitudeDegrees))
                       + tlv(0x11, leDouble(longitudeDegrees))
                       + tlv(0x12, leFloat(uncertainty))
                       + tlv(0x13, le<quint8>(68))
                       + tlv(0x1B, le<quint64>(quint64(timestamp) * 1000)));
}

void QmiLocationBackend::gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags)
{
    Q_UNUSED(aidingDataFlags)
    qCDebug(lcGeoclueHybris) << "QMI LOC: deleting aiding data is not implemented";
}

bool QmiLocationBackend::gnssSetPositionMode(HybrisGnssPositionMode mode,
                                             HybrisGnssPositionRecurrence recurrence,
                                             uint32_t minIntervalMs,
                                             uint32_t preferredAccuracyMeters,
                                             uint32_t preferredTimeMs)
{
    Q_UNUSED(mode)              // standalone only: no SUPL path without a data connection
    Q_UNUSED(recurrence)        // the provider only asks for periodic fixes
    Q_UNUSED(preferredAccuracyMeters)
    Q_UNUSED(preferredTimeMs)

    m_minIntervalMs = qMax<uint32_t>(minIntervalMs, 1000);
    return true;
}

void QmiLocationBackend::gnssDebugInit()
{
}

void QmiLocationBackend::gnssNiInit()
{
}

void QmiLocationBackend::gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse)
{
    Q_UNUSED(notifId)
    Q_UNUSED(userResponse)
}

void QmiLocationBackend::gnssXtraInit()
{
}

bool QmiLocationBackend::gnssXtraInjectXtraData(QByteArray &xtraData)
{
    if (!m_haveService || xtraData.isEmpty())
        return false;
    // the modem takes the next part only after the indication for this one;
    // it checks the assembled file at the last part
    m_xtraData = xtraData;
    m_xtraPart = 0;
    return sendXtraPart();
}

bool QmiLocationBackend::sendXtraPart()
{
    const int parts = (m_xtraData.size() + XtraPartSize - 1) / XtraPartSize;
    const QByteArray part = m_xtraData.mid(m_xtraPart * XtraPartSize, XtraPartSize);
    ++m_xtraPart;
    return sendRequest(LocInjectXtraData,
                       tlv(0x01, le<quint32>(quint32(m_xtraData.size())))
                       + tlv(0x02, le<quint16>(quint16(parts)))
                       + tlv(0x03, le<quint16>(quint16(m_xtraPart)))
                       + tlv(0x04, le<quint16>(quint16(part.size())) + part));
}

void QmiLocationBackend::handleXtraInjected(const Tlvs &tlvs)
{
    if (m_xtraData.isEmpty())
        return;
    quint32 status = 1;
    readLe(tlvs.value(0x01), 0, &status);
    const int parts = (m_xtraData.size() + XtraPartSize - 1) / XtraPartSize;
    if (status != 0) {
        quint16 code = 0;
        readLe(tlvs.value(0x10), 0, &code);
        qWarning("QMI LOC: XTRA part %d of %d refused, status %u, 0x10 %u (engine locked?)",
                 m_xtraPart, parts, status, code);
        m_xtraData.clear();
    } else if (m_xtraPart < parts) {
        sendXtraPart();
    } else {
        qCDebug(lcGeoclueHybris) << "QMI LOC: XTRA injected," << m_xtraData.size() << "bytes";
        m_xtraData.clear();
        sendRequest(LocGetPredictedOrbitsValidity, QByteArray());
    }
}

void QmiLocationBackend::handleXtraValidity(const Tlvs &tlvs)
{
    quint32 status = 1;
    readLe(tlvs.value(0x01), 0, &status);
    quint64 start = 0;
    quint16 hours = 0;
    // 0x10: start of the validity window in UTC seconds, and its length in hours
    if (status != 0 || !readLe(tlvs.value(0x10), 0, &start) || !readLe(tlvs.value(0x10), 8, &hours)) {
        qCDebug(lcGeoclueHybris) << "QMI LOC: no XTRA data, requesting a download";
        QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
        return;
    }
    const qint64 left = qint64(start) + qint64(hours) * 3600 - QDateTime::currentMSecsSinceEpoch() / 1000;
    qCDebug(lcGeoclueHybris) << "QMI LOC: XTRA valid for" << left / 3600 << "more hours";
    if (left < XtraRefreshSeconds)
        QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
}

void QmiLocationBackend::aGnssInit()
{
}

bool QmiLocationBackend::aGnssDataConnClosed()
{
    return false;
}

bool QmiLocationBackend::aGnssDataConnFailed()
{
    return false;
}

bool QmiLocationBackend::aGnssDataConnOpen(const QByteArray &apn, const QString &protocol)
{
    Q_UNUSED(apn)
    Q_UNUSED(protocol)
    return false;
}

int QmiLocationBackend::aGnssSetServer(HybrisAGnssType type, const char *hostname, int port)
{
    Q_UNUSED(type)
    Q_UNUSED(hostname)
    Q_UNUSED(port)
    return -1;
}

void QmiLocationBackend::aGnssRilInit()
{
}

bool QmiLocationBackend::openSocket()
{
    m_fd = socket(AF_QIPCRTR, SOCK_DGRAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (m_fd < 0) {
        qWarning("QMI LOC: cannot open a QRTR socket: %s", strerror(errno));
        return false;
    }

    sockaddr_qrtr local;
    socklen_t length = sizeof(local);
    memset(&local, 0, sizeof(local));
    if (getsockname(m_fd, reinterpret_cast<sockaddr *>(&local), &length) < 0) {
        qWarning("QMI LOC: getsockname failed: %s", strerror(errno));
        closeSocket();
        return false;
    }
    m_localNode = local.sq_node;

    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &QmiLocationBackend::readSocket);
    return true;
}

void QmiLocationBackend::closeSocket()
{
    delete m_notifier;
    m_notifier = nullptr;
    if (m_fd >= 0)
        close(m_fd);
    m_fd = -1;
    m_haveService = false;
}

void QmiLocationBackend::lookupService()
{
    // the lookup stays registered: the name service also reports when the modem
    // restarts (DEL_SERVER) and when the service is back (NEW_SERVER)
    QByteArray packet = le<quint32>(QrtrTypeNewLookup) + le<quint32>(LocService)
            + le<quint32>(0) + le<quint32>(0) + le<quint32>(0);

    sockaddr_qrtr control;
    memset(&control, 0, sizeof(control));
    control.sq_family = AF_QIPCRTR;
    control.sq_node = m_localNode;
    control.sq_port = QRTR_PORT_CTRL;
    if (sendto(m_fd, packet.constData(), packet.size(), 0,
               reinterpret_cast<sockaddr *>(&control), sizeof(control)) < 0) {
        qWarning("QMI LOC: service lookup failed: %s", strerror(errno));
    }
}

bool QmiLocationBackend::sendRequest(quint16 messageId, const QByteArray &tlvs)
{
    if (m_fd < 0 || !m_haveService)
        return false;

    QByteArray packet(7, Qt::Uninitialized);
    packet[0] = char(QmiRequest);
    qToLittleEndian<quint16>(++m_transaction, reinterpret_cast<uchar *>(packet.data() + 1));
    qToLittleEndian<quint16>(messageId, reinterpret_cast<uchar *>(packet.data() + 3));
    qToLittleEndian<quint16>(quint16(tlvs.size()), reinterpret_cast<uchar *>(packet.data() + 5));
    packet += tlvs;

    sockaddr_qrtr service;
    memset(&service, 0, sizeof(service));
    service.sq_family = AF_QIPCRTR;
    service.sq_node = m_node;
    service.sq_port = m_port;
    if (sendto(m_fd, packet.constData(), packet.size(), 0,
               reinterpret_cast<sockaddr *>(&service), sizeof(service)) < 0) {
        qWarning("QMI LOC: request 0x%04x failed: %s", messageId, strerror(errno));
        return false;
    }
    return true;
}

void QmiLocationBackend::serviceArrived(quint32 node, quint32 port)
{
    qCDebug(lcGeoclueHybris) << "QMI LOC service on node" << node << "port" << port;
    m_node = node;
    m_port = port;
    m_haveService = true;
    if (m_sessionWanted)
        registerAndStart();
}

void QmiLocationBackend::serviceLeft()
{
    qCDebug(lcGeoclueHybris) << "QMI LOC service gone (modem restart?)";
    m_haveService = false;
    QMetaObject::invokeMethod(staticProvider, "engineOff", Qt::QueuedConnection);
}

void QmiLocationBackend::registerAndStart()
{
    // registration belongs to this client and is lost with a modem restart: send it each time
    const quint64 events = EventPositionReport | EventGnssSatelliteInfo | EventInjectTimeRequest
            | EventInjectPredictedOrbitsRequest | EventEngineState | EventFixSessionState;
    sendRequest(LocRegisterEvents, tlv(0x01, le<quint64>(events)));

    qCDebug(lcGeoclueHybris) << "QMI LOC start, interval" << m_minIntervalMs << "ms";
    sendRequest(LocStart, tlv(0x01, le<quint8>(SessionId))
                + tlv(0x10, le<quint32>(RecurrencePeriodic))
                + tlv(0x12, le<quint32>(IntermediateReportsOff))
                + tlv(0x13, le<quint32>(m_minIntervalMs)));
    // a file older than a day is replaced (the provider downloads only while online
    // assistance is allowed in the location settings)
    sendRequest(LocGetPredictedOrbitsValidity, QByteArray());
}

void QmiLocationBackend::readSocket()
{
    char buffer[65536];
    for (;;) {
        sockaddr_qrtr source;
        socklen_t length = sizeof(source);
        const ssize_t size = recvfrom(m_fd, buffer, sizeof(buffer), 0,
                                      reinterpret_cast<sockaddr *>(&source), &length);
        if (size < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                return;
            // e.g. ENETRESET: start over with a fresh socket and lookup
            qWarning("QMI LOC: socket error %s, reopening", strerror(errno));
            const bool wanted = m_sessionWanted;
            closeSocket();
            m_sessionWanted = wanted;
            if (openSocket())
                lookupService();
            return;
        }

        const QByteArray packet(buffer, int(size));
        if (source.sq_port == QRTR_PORT_CTRL)
            handleControl(packet);
        else if (m_haveService && source.sq_node == m_node && source.sq_port == m_port)
            handleQmi(packet);
    }
}

void QmiLocationBackend::handleControl(const QByteArray &packet)
{
    quint32 type, service, instance, node, port;
    if (!readLe(packet, 0, &type) || !readLe(packet, 4, &service) || !readLe(packet, 8, &instance)
            || !readLe(packet, 12, &node) || !readLe(packet, 16, &port)) {
        return;
    }
    Q_UNUSED(instance)

    if (service != LocService)
        return;    // includes the empty NEW_SERVER that ends the lookup list

    if (type == QrtrTypeNewServer)
        serviceArrived(node, port);
    else if (type == QrtrTypeDelServer && m_haveService && node == m_node && port == m_port)
        serviceLeft();
}

void QmiLocationBackend::handleQmi(const QByteArray &packet)
{
    quint16 messageId, length;
    if (packet.size() < 7 || !readLe(packet, 3, &messageId) || !readLe(packet, 5, &length))
        return;
    const quint8 type = quint8(packet.at(0));

    Tlvs tlvs;
    int offset = 7;
    const int end = qMin(packet.size(), 7 + int(length));
    while (offset + 3 <= end) {
        quint16 size;
        if (!readLe(packet, offset + 1, &size) || offset + 3 + size > end)
            break;
        tlvs.insert(quint8(packet.at(offset)), packet.mid(offset + 3, size));
        offset += 3 + size;
    }

    if (type == QmiResponse) {
        quint16 result = 0, error = 0;
        readLe(tlvs.value(0x02), 0, &result);
        readLe(tlvs.value(0x02), 2, &error);
        if (result != 0)
            qWarning("QMI LOC: request 0x%04x failed, QMI error %u", messageId, error);
        return;
    }
    if (type != QmiIndication)
        return;

    quint32 value = 0;
    switch (messageId) {
    case LocPositionReport:
        handlePosition(tlvs);
        break;
    case LocGnssSvInfo:
        handleSatellites(tlvs);
        break;
    case LocEngineState:
        if (readLe(tlvs.value(0x01), 0, &value)) {
            qCDebug(lcGeoclueHybris) << "QMI LOC engine state" << value;
            if (value == EngineStateOn)
                QMetaObject::invokeMethod(staticProvider, "engineOn", Qt::QueuedConnection);
            else if (value == EngineStateOff)
                QMetaObject::invokeMethod(staticProvider, "engineOff", Qt::QueuedConnection);
        }
        break;
    case LocInjectTimeRequest:
        qCDebug(lcGeoclueHybris) << "QMI LOC requests UTC time";
        QMetaObject::invokeMethod(staticProvider, "injectUtcTime", Qt::QueuedConnection);
        break;
    case LocInjectPredictedOrbitsRequest:
        qCDebug(lcGeoclueHybris) << "QMI LOC requests XTRA data";
        QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
        break;
    case LocGetPredictedOrbitsValidity:
        handleXtraValidity(tlvs);
        break;
    case LocInjectXtraData:
        handleXtraInjected(tlvs);
        break;
    case LocInjectUtcTime:
    case LocInjectPosition:
        if (readLe(tlvs.value(0x01), 0, &value))
            qCDebug(lcGeoclueHybris) << "QMI LOC injection" << hex << messageId << "status" << value;
        break;
    default:
        break;
    }
}

void QmiLocationBackend::handlePosition(const Tlvs &tlvs)
{
    // the satellites in this fix, for the next satellite report
    const QByteArray used = tlvs.value(0x2C);
    if (!used.isEmpty()) {
        m_usedPrns.clear();
        const int count = quint8(used.at(0));
        for (int i = 0; i < count; ++i) {
            quint16 id;
            if (readLe(used, 1 + 2 * i, &id))
                m_usedPrns.append(id);
        }
    }

    quint32 status;
    if (!readLe(tlvs.value(0x01), 0, &status) || status != SessionStatusSuccess)
        return;

    double latitude, longitude;
    if (!readDouble(tlvs.value(0x10), &latitude) || !readDouble(tlvs.value(0x11), &longitude))
        return;

    Location location;
    quint64 timestamp;
    location.setTimestamp(readLe(tlvs.value(0x25), 0, &timestamp)
                          ? qint64(timestamp) : QDateTime::currentMSecsSinceEpoch());
    location.setLatitude(latitude);
    location.setLongitude(longitude);

    float value;
    if (readFloat(tlvs.value(0x1A), 0, &value))      // above the WGS84 ellipsoid, as Android
        location.setAltitude(value);
    if (readFloat(tlvs.value(0x18), 0, &value))      // m/s, the provider takes knots
        location.setSpeed(value * MpsToKnots);
    if (readFloat(tlvs.value(0x20), 0, &value))
        location.setDirection(value);
    if (readFloat(tlvs.value(0x1F), 0, &value))
        location.setClimb(value);

    Accuracy accuracy;
    if (readFloat(tlvs.value(0x12), 0, &value))      // circular horizontal uncertainty
        accuracy.setHorizontal(value);
    if (readFloat(tlvs.value(0x1C), 0, &value))
        accuracy.setVertical(value);
    location.setAccuracy(accuracy);

    QMetaObject::invokeMethod(staticProvider, "setLocation", Qt::QueuedConnection,
                              Q_ARG(Location, location));
}

void QmiLocationBackend::handleSatellites(const Tlvs &tlvs)
{
    const QByteArray list = tlvs.value(0x10);
    QList<SatelliteInfo> satellites;
    if (!list.isEmpty()) {
        const int count = quint8(list.at(0));
        for (int i = 0; i < count; ++i) {
            const int base = 1 + i * SvEntrySize;
            quint32 valid;
            quint16 id;
            if (!readLe(list, base, &valid) || !readLe(list, base + 8, &id))
                break;
            if (!(valid & SvValidSatelliteId))
                continue;

            // QMI LOC numbers satellites per system the way geoclue-hybris does
            // (GLONASS 65-96, BeiDou 201-, Galileo 301-), so the id is the PRN
            SatelliteInfo info;
            info.setPrn(id);
            float value;
            if ((valid & SvValidElevation) && readFloat(list, base + 16, &value))
                info.setElevation(int(value));
            if ((valid & SvValidAzimuth) && readFloat(list, base + 20, &value))
                info.setAzimuth(int(value));
            if ((valid & SvValidSnr) && readFloat(list, base + 24, &value))
                info.setSnr(int(value));
            satellites.append(info);
        }
    }

    QMetaObject::invokeMethod(staticProvider, "setSatellite", Qt::QueuedConnection,
                              Q_ARG(QList<SatelliteInfo>, satellites),
                              Q_ARG(QList<int>, m_usedPrns));
}
