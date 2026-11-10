// SPDX-License-Identifier: GPL-3.0-or-later
#include "mafpbackend.h"

#include <QDebug>

#include <chrono>
#include <cstring>
#include <ctime>

namespace {

/* Where the TA keeps its templates, resolved by qsee-supplicant under its state directory. */
const char *const GROUP_PATH = "/data/system/users/0/fpdata";
const uint32_t GROUP_ID = 0;

/* Pro1X: TLMM GPIO 108 powers the sensor ("microarray,gpio_pwr" in the stock DT). */
const char *const POWER_CHIP_LABEL = "500000.pinctrl";
const unsigned int POWER_LINE = 108;

const double ENROLL_TOUCH_TIMEOUT = 90;     /* s without a finger before the enrolment fails */
const double LIFT_TIMEOUT = 5;              /* s to wait for the finger to leave */
const int POLL_MS = 30;                     /* detect_touch poll, as the Android HAL's enroll loop */

QList<uint32_t> toList(const std::vector<uint32_t> &v)
{
    QList<uint32_t> l;
    for (uint32_t x : v)
        l.append(x);
    return l;
}

double now()
{
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}

MafpBackend::MafpBackend(QObject *parent)
    : QObject(parent), m_busy(false), m_cancel(false)
{
}

MafpBackend::~MafpBackend()
{
    m_cancel = true;
    if (m_worker.joinable())
        m_worker.join();
    m_ta.stop();
}

bool MafpBackend::start()
{
    if (!m_power.held() && !m_power.hold(POWER_CHIP_LABEL, POWER_LINE, true)) {
        qWarning() << "sensor power line" << POWER_LINE << "on" << POWER_CHIP_LABEL
                   << "not available:" << strerror(errno);
        return false;
    }
    if (!m_ta.start(GROUP_PATH, GROUP_ID)) {
        qWarning() << "mafp not started:" << lastError();
        return false;
    }
    qInfo() << "mafp up, chip id 0x" + QString::number(m_ta.chipId(), 16);
    return true;
}

bool MafpBackend::begin(const char *what)
{
    if (!m_ta.isStarted()) {
        emit failed(QStringLiteral("HW_UNAVAILABLE"));
        return false;
    }
    if (m_busy.exchange(true)) {
        qWarning() << what << "while busy";
        emit failed(QStringLiteral("BUSY"));
        return false;
    }
    if (m_worker.joinable())
        m_worker.join();
    m_cancel = false;
    return true;
}

void MafpBackend::finish()
{
    m_busy = false;
}

bool MafpBackend::sleepStep()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(POLL_MS));
    return !m_cancel;
}

/* Poll detect_touch(mode) until it reports `touched`, a cancel or the timeout. */
bool MafpBackend::waitTouch(int mode, bool touched, double seconds)
{
    double end = now() + seconds;
    while (now() < end) {
        int t = m_ta.detectTouch(mode);
        if (t < 0) {
            qWarning() << "detect_touch failed:" << lastError();
            return false;
        }
        if ((t != 0) == touched)
            return true;
        if (!sleepStep())
            return false;
    }
    return false;
}

void MafpBackend::enroll()
{
    if (!begin("enroll"))
        return;
    m_worker = std::thread(&MafpBackend::runEnroll, this);
}

/*
 * As enroll_work in the Android HAL: has_calibrate (calibrate), pre_enroll,
 * enroll_start with a zero auth token (accepted by this TA), then per sample
 * a touch, detectTouch_getEnergy, do_enroll, and the lift; enroll_finish
 * after ENROLL_SAMPLES accepted samples. The new template id is the one
 * enumerate shows afterwards that was not there before.
 */
void MafpBackend::runEnroll()
{
    int32_t z = 0;
    uint8_t params[0x48];
    memset(params, 0, sizeof(params));
    QList<uint32_t> before = toList(m_ta.enumerate());
    const int32_t *r;

    r = m_ta.command(MafpTa::CMD_HAS_CALIBRATE, &z, 4, 8);
    if (r && r[0]) {
        r = m_ta.command(MafpTa::CMD_CALIBRATE, &z, 4, 8);
        qInfo() << "calibrate: ret" << (r ? r[0] : -99) << "err" << (r ? r[1] : -99);
    }
    r = m_ta.command(MafpTa::CMD_PRE_ENROLL, &z, 4, 8);
    r = m_ta.command(MafpTa::CMD_ENROLL_START, params, sizeof(params), sizeof(params));
    if (!r || r[1]) {
        qWarning() << "enroll_start failed:" << (r ? QString::number(r[1]) : lastError());
        emit failed(QStringLiteral("UNABLE_TO_PROCESS"));
        finish();
        return;
    }
    int32_t startFid = r[0];
    qInfo() << "enrolment started, fid" << startFid;

    int accepted = 0, tries = 0;
    while (accepted < MafpTa::ENROLL_SAMPLES) {
        if (!waitTouch(1, true, ENROLL_TOUCH_TIMEOUT)) {
            m_ta.command(MafpTa::CMD_CANCEL, &z, 4, 8);
            m_ta.command(MafpTa::CMD_ENROLL_FINISH, &z, 4, 8);
            if (!m_cancel)
                emit failed(QStringLiteral("TIMEOUT"));
            finish();
            return;
        }
        r = m_ta.command(MafpTa::CMD_TOUCH_ENERGY, nullptr, 0, 0x3c0c);
        int energy = r ? r[1] : -99;

        uint8_t in[0x14];
        uint32_t ts = time(nullptr);
        memset(in, 0, sizeof(in));
        memcpy(in + 0x10, &ts, 4);
        r = m_ta.command(MafpTa::CMD_DO_ENROLL, in, sizeof(in), 0x16810);
        tries++;
        if (!r) {
            emit failed(QStringLiteral("UNABLE_TO_PROCESS"));
            finish();
            return;
        }
        int32_t status = r[1], err = r[2];
        qInfo() << "sample" << tries << "energy" << energy << "status" << status << "err" << err;
        if (!status && !err) {
            accepted++;
            emit acquired(AcquiredGood);
            emit enrollProgress(static_cast<float>(accepted) / MafpTa::ENROLL_SAMPLES);
        } else {
            /* -2988 was the one rejection seen on 2026-11-09; the other codes are not mapped yet */
            emit acquired(AcquiredInsufficient);
        }
        waitTouch(1, false, LIFT_TIMEOUT);
        if (m_cancel) {
            m_ta.command(MafpTa::CMD_CANCEL, &z, 4, 8);
            m_ta.command(MafpTa::CMD_ENROLL_FINISH, &z, 4, 8);
            finish();
            return;
        }
    }
    r = m_ta.command(MafpTa::CMD_ENROLL_FINISH, &z, 4, 8);
    qInfo() << "enroll_finish: ret" << (r ? r[0] : -99) << "err" << (r ? r[1] : -99)
            << "after" << tries << "samples";

    QList<uint32_t> after = toList(m_ta.enumerate());
    uint32_t fid = 0;
    for (uint32_t f : after)
        if (!before.contains(f))
            fid = f;
    if (!fid && after.contains(startFid))
        fid = startFid;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fingers = after;
    }
    if (fid) {
        emit enrollProgress(1);
        emit succeeded(fid);
    } else {
        qWarning() << "no new template after enrolment; store has" << after;
        emit failed(QStringLiteral("UNABLE_TO_PROCESS"));
    }
    finish();
}

void MafpBackend::authenticate()
{
    if (!begin("authenticate"))
        return;
    m_worker = std::thread(&MafpBackend::runAuthenticate, this);
}

/*
 * As tap_work_active: reset/calc interrupt parameters, enumerate,
 * update_authenticate_data, then per attempt a touch (mode 0), the chip
 * 0x24 capture (mode 4, up to five times) and do_verify. Reply: [0] template
 * count, [4] matched id (0 = none), [0xc] status, [0x10] err. Runs until a
 * match, a cancel or a transport failure; "FINGER_NOT_RECOGNIZED" per miss.
 *
 * State of knowledge (2026-11-09): six attempts with an enrolled finger all
 * came back with id 0 and a zero auth token in the request. Whether the
 * token, the capture mode or the retry word is what is missing is open; this
 * code sends what the Android library sends as far as it is mapped.
 */
void MafpBackend::runAuthenticate()
{
    int32_t z = 0;
    const int32_t *r;

    m_ta.command(MafpTa::CMD_RESET_INTERRUPT_PARA, &z, 4, 8);
    m_ta.command(MafpTa::CMD_CALC_INTERRUPT_PARA, &z, 4, 8);
    m_ta.enumerate();
    r = m_ta.command(MafpTa::CMD_UPDATE_AUTH_DATA, &z, 4, 4);
    if (!r) {
        emit failed(QStringLiteral("UNABLE_TO_PROCESS"));
        finish();
        return;
    }

    uint32_t retry = 1;
    while (!m_cancel) {
        if (!waitTouch(0, true, 3600)) {
            if (!m_cancel)
                emit failed(QStringLiteral("UNABLE_TO_PROCESS"));
            finish();
            return;
        }
        int32_t m4 = 4, got = 0;
        for (int k = 0; k < 5 && !got; k++) {
            r = m_ta.command(MafpTa::CMD_DETECT_TOUCH, &m4, 4, 0x16808);
            got = r && r[0] == 1;
        }
        uint8_t in[0x60];
        uint32_t ts = time(nullptr);
        memset(in, 0, sizeof(in));
        memcpy(in + 0xc, &retry, 4);
        memcpy(in + 0x10, &ts, 4);
        r = m_ta.command(MafpTa::CMD_DO_VERIFY, in, sizeof(in), 0x16860);
        if (!r) {
            emit failed(QStringLiteral("UNABLE_TO_PROCESS"));
            finish();
            return;
        }
        int32_t count = r[0], fid = r[1], status = r[3], err = r[4];
        qInfo() << "verify: count" << count << "fid" << fid << "status" << status << "err" << err
                << "capture" << got;
        if (fid) {
            emit acquired(AcquiredGood);
            emit authenticated(fid);
            finish();
            return;
        }
        emit acquired(AcquiredGood);
        emit failed(QStringLiteral("FINGER_NOT_RECOGNIZED"));
        retry++;
        waitTouch(0, false, LIFT_TIMEOUT);
    }
    finish();
}

void MafpBackend::enumerate()
{
    if (!begin("enumerate"))
        return;
    m_worker = std::thread(&MafpBackend::runEnumerate, this);
}

void MafpBackend::runEnumerate()
{
    QList<uint32_t> fids = toList(m_ta.enumerate());
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fingers = fids;
    }
    qInfo() << "templates on file:" << fids;
    emit enumerated();
    finish();
}

void MafpBackend::remove(uint32_t finger)
{
    if (!begin("remove"))
        return;
    m_worker = std::thread(&MafpBackend::runRemove, this, finger, false);
}

void MafpBackend::clear()
{
    if (!begin("clear"))
        return;
    m_worker = std::thread(&MafpBackend::runRemove, this, 0u, true);
}

void MafpBackend::runRemove(uint32_t finger, bool all)
{
    QList<uint32_t> fids = toList(m_ta.enumerate());
    bool ok = true;
    for (uint32_t f : fids) {
        if (!all && f != finger)
            continue;
        if (!m_ta.remove(f)) {
            qWarning() << "remove" << f << "failed:" << lastError();
            ok = false;
        }
    }
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_fingers = toList(m_ta.enumerate());
    }
    if (ok)
        emit removed(all ? 0 : finger);
    else
        emit failed(QStringLiteral("UNABLE_TO_REMOVE"));
    finish();
}

void MafpBackend::cancel()
{
    m_cancel = true;
}

QList<uint32_t> MafpBackend::fingerprints() const
{
    std::lock_guard<std::mutex> lock(const_cast<std::mutex &>(m_mutex));
    return m_fingers;
}
