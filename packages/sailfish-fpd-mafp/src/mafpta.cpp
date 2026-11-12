// SPDX-License-Identifier: GPL-3.0-or-later
#include "mafpta.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

namespace {

std::string hex(uint32_t v)
{
    char b[16];
    snprintf(b, sizeof(b), "0x%x", v);
    return b;
}

}

namespace {

const uint32_t MA_DRV_VERSION = 0x4005;         /* madev ioctl 'M' 0x23 on Android */
const char *const KEYMASTER_APP = "keymaster64";
const uint32_t KEYMASTER_GET_AUTH_TOKEN_KEY = 0x205;
const char *const MAFP_APP = "mafp";

}

bool MafpTa::fail(const std::string &what)
{
    m_error = what;
    if (errno)
        m_error += std::string(": ") + strerror(errno);
    return false;
}

/*
 * keymaster64 command 0x205 (GET_AUTH_TOKEN_KEY) with type 2: a 64-byte
 * request [u32 0x205][u32 2], a 0x3c0 reply [status][offset][len][key].
 * The library seeds the fingerprint application with this key at INIT; the
 * key is never logged.
 */
bool MafpTa::keymasterKey(std::vector<uint8_t> *key)
{
    TeeShm req, rsp;
    uint32_t session;

    if (!req.alloc(m_tee.fd(), 64) || !rsp.alloc(m_tee.fd(), 0x3c0))
        return fail("keymaster shared memory");
    if (!m_tee.openSession(KEYMASTER_APP, &session))
        return fail("keymaster64 session");
    reinterpret_cast<uint32_t *>(req.data())[0] = KEYMASTER_GET_AUTH_TOKEN_KEY;
    reinterpret_cast<uint32_t *>(req.data())[1] = 2;
    int ret = m_tee.invoke(session, req, 64, rsp, 0x3c0);
    m_tee.closeSession(session);
    if (ret)
        return fail("keymaster64 0x205 returned " + std::to_string(ret));
    const uint32_t *r = reinterpret_cast<const uint32_t *>(rsp.data());
    uint32_t status = r[0], len = r[2];
    if (status || !len || len > 0x400) {
        errno = 0;
        return fail("keymaster64 gave no usable key (status " +
                    std::to_string(static_cast<int32_t>(status)) + ")");
    }
    key->assign(rsp.data() + 12, rsp.data() + 12 + len);
    rsp.clear();
    return true;
}

/* g_config as init_properties fills it for chip 0x24 from the stock defaults. */
void MafpTa::fillConfig(uint8_t *c) const
{
    static const struct { uint16_t off; int32_t v; } f[] = {
        { 0x000, 11 },      /* ro.build.version.release of the stock Android */
        { 0x004, 1 },       /* enable.navi */
        { 0x008, 1 },       /* return.auth */
        { 0x00c, 1 },       /* duplicate.enroll */
        { 0x010, 0 },       /* part.touch */
        { 0x014, ENROLL_SAMPLES }, /* enroll.times */
        { 0x018, 2 },       /* match.time */
        { 0x01c, 900 },     /* lpress.time (ms) */
        { 0x024, 0 },       /* energy.th, the TA sets 300 */
        { 0x038, 2 },       /* enroll.fast.num */
        { 0x138, 1 },       /* dirty_check */
        { 0x13c, 0 },       /* save.debug.bmp */
        { 0x144, 5 },       /* enroll_dupl_arae */
        { 0x148, 80 },      /* part_touch_ratio */
    };
    for (size_t i = 0; i < sizeof(f) / sizeof(f[0]); i++)
        memcpy(c + f[i].off, &f[i].v, 4);
    /* +0xc0 ro.serialno: left empty */
}

const int32_t *MafpTa::command(uint32_t cmd, const void *payload, size_t plen, size_t len)
{
    if (!m_haveSession || len > BUF_SIZE - 8 || plen > len) {
        errno = EINVAL;
        fail("command " + hex(cmd) + " on a closed session");
        return nullptr;
    }
    memset(m_req.data(), 0, len + 8);
    memcpy(m_req.data(), &cmd, 4);
    uint32_t l = len;
    memcpy(m_req.data() + 4, &l, 4);
    if (payload && plen)
        memcpy(m_req.data() + 8, payload, plen);
    int ret = m_tee.invoke(m_session, m_req, BUF_SIZE, m_rsp, BUF_SIZE);
    if (ret) {
        if (ret > 0)
            errno = 0;
        m_transportFailures++;
        fail("mafp command " + hex(cmd) + " failed, ret " + std::to_string(ret));
        return nullptr;
    }
    m_transportFailures = 0;
    return reinterpret_cast<const int32_t *>(m_req.data() + 8);
}

bool MafpTa::start(const std::string &groupPath, uint32_t gid)
{
    std::vector<uint8_t> key;

    stop();
    errno = 0;
    if (!m_tee.open())
        return fail("no QSEECOM TEE device (qseecomtee loaded?)");
    if (!m_req.alloc(m_tee.fd(), BUF_SIZE) || !m_rsp.alloc(m_tee.fd(), BUF_SIZE))
        return fail("mafp shared memory");
    if (!keymasterKey(&key))
        return false;
    if (!m_tee.openSession(MAFP_APP, &m_session))
        return fail("mafp session (is qsee-app-loader@mafp running?)");
    m_haveSession = true;

    /* INIT: [driver version][key length][key]; the first command must be this one. */
    std::vector<uint8_t> init(0x408, 0);
    uint32_t klen = key.size();
    memcpy(&init[0], &MA_DRV_VERSION, 4);
    memcpy(&init[4], &klen, 4);
    memcpy(&init[8], key.data(), key.size());
    const int32_t *r = command(CMD_INIT, init.data(), init.size(), init.size());
    std::fill(init.begin(), init.end(), 0);
    std::fill(key.begin(), key.end(), 0);
    m_req.clear();
    if (!r)
        return false;

    r = command(CMD_CHIP_ID, nullptr, 0, 0xc);
    if (!r)
        return false;
    m_chipId = r[0];
    if (r[1]) {
        errno = 0;
        return fail("chip id read failed, err " + std::to_string(r[1]) +
                    " (sensor power GPIO low?)");
    }

    std::vector<uint8_t> cfg(0x180, 0);
    fillConfig(cfg.data());
    r = command(CMD_SERVICE_INIT, cfg.data(), cfg.size(), cfg.size());
    if (!r)
        return false;
    /* The TA writes its git tag at +0x154 and an error word at +0x150. */
    if (r[0x150 / 4]) {
        errno = 0;
        return fail("service init error " + std::to_string(r[0x150 / 4]));
    }

    /* SET_ACTIVE_GROUP: the same g_config with the store path at +0x3c and the gid at +0xbc. */
    std::vector<uint8_t> grp(0x154, 0);
    fillConfig(grp.data());
    strncpy(reinterpret_cast<char *>(&grp[0x3c]), groupPath.c_str(), 0x7f);
    memcpy(&grp[0xbc], &gid, 4);
    r = command(CMD_SET_ACTIVE_GROUP, grp.data(), grp.size(), grp.size());
    if (!r)
        return false;
    if (r[0x150 / 4]) {
        errno = 0;
        return fail("set active group error " + std::to_string(r[0x150 / 4]));
    }

    m_started = true;
    m_error.clear();
    return true;
}

void MafpTa::stop()
{
    if (m_haveSession)
        m_tee.closeSession(m_session);
    m_haveSession = false;
    m_started = false;
    m_transportFailures = 0;
    m_tee.close();
}

int MafpTa::detectTouch(int mode)
{
    int32_t m = mode;
    const int32_t *r = command(CMD_DETECT_TOUCH, &m, 4, 0x16808);
    return r ? r[0] : -1;
}

std::vector<uint32_t> MafpTa::enumerate()
{
    std::vector<uint32_t> fids;
    int32_t z = 0;
    const int32_t *r = command(CMD_ENUMERATE, &z, 4, 0x20);
    if (!r)
        return fids;
    int count = r[0];
    for (int i = 0; i < count && i < 5; i++)
        if (r[3 + i])
            fids.push_back(r[3 + i]);
    return fids;
}

bool MafpTa::remove(uint32_t fid)
{
    int32_t f = fid;
    const int32_t *r = command(CMD_REMOVE, &f, 4, 0x20);
    return r && r[1] == 0;
}
