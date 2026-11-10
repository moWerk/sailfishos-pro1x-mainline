// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The MicroArray fingerprint trusted application ("mafp", TZ name
 * "fingerprint") of the F(x)tec Pro1X, spoken to the way its Android client
 * library (libfprint-x64.so) does. Command ids, payload sizes and reply
 * layouts are from the protocol map of 2026-11-09 (port notes,
 * fingerprint); everything here is blocking and belongs on a worker thread.
 *
 * Shared buffer layout: [u32 cmd][u32 len][payload]. The application
 * answers in place, so the reply payload is read from the request buffer
 * after the send (the driver copies an INOUT request back).
 */
#ifndef MAFPTA_H
#define MAFPTA_H

#include "teeclient.h"

#include <cstdint>
#include <string>
#include <vector>

class MafpTa
{
public:
    enum {
        CMD_INIT = 0xc8,            /* [drv version][key len][key] */
        CMD_SERVICE_INIT = 0xc9,    /* g_config, 0x180 */
        CMD_SET_ACTIVE_GROUP = 0xca,/* g_config with path, 0x154 */
        CMD_PRE_ENROLL = 0xcd,
        CMD_ENROLL_START = 0xce,    /* 0x48: hw_auth_token_t */
        CMD_DO_ENROLL = 0xcf,       /* 0x16810 */
        CMD_ENROLL_FINISH = 0xd0,
        CMD_CANCEL = 0xd1,
        CMD_DETECT_TOUCH = 0xd2,    /* 0x16808, [mode] -> [touched][err] */
        CMD_TOUCH_ENERGY = 0xd3,    /* 0x3c0c */
        CMD_GET_ENERGY = 0xd4,      /* 0x16814 */
        CMD_DO_VERIFY = 0xd5,       /* 0x16860 */
        CMD_UPDATE_AUTH_DATA = 0xd6,
        CMD_REMOVE = 0xd7,          /* 0x20: [fid] -> [.][err][left][fids] */
        CMD_HAS_CALIBRATE = 0xd8,
        CMD_CALIBRATE = 0xd9,
        CMD_CALC_INTERRUPT_PARA = 0xda,
        CMD_RESET_INTERRUPT_PARA = 0xe1,
        CMD_ENUMERATE = 0xe2,       /* 0x20: -> [count][.][err][fids 0..4] */
        CMD_CHIP_ID = 0xef,         /* 0xc: -> [chip][err][cover] */
    };

    static const size_t BUF_SIZE = 0x20000;
    static const int ENROLL_SAMPLES = 14;       /* persist.vendor.fp.enroll.times */

    MafpTa() = default;

    /*
     * Bring the application up as bio_init does: the auth-token key from
     * keymaster64, then INIT, CHIP_ID, SERVICE_INIT, SET_ACTIVE_GROUP. The
     * application must be loaded (qsee-app-loader@mafp) and the supplicant
     * running. Returns false with an explanation in error().
     */
    bool start(const std::string &groupPath, uint32_t gid);
    void stop();
    bool isStarted() const { return m_started; }
    int chipId() const { return m_chipId; }
    const std::string &error() const { return m_error; }

    /*
     * One command. payload/plen is what is placed at the start of a len-byte
     * payload (the rest zero). Returns the in-place reply payload as 32-bit
     * words, or nullptr on a transport failure (error() set). The pointer
     * is into the shared buffer and valid until the next command.
     */
    const int32_t *command(uint32_t cmd, const void *payload, size_t plen, size_t len);

    /* Conveniences over command(); -1 or empty on a transport failure. */
    int detectTouch(int mode);               /* reply[0]: 1 touched, 0 not */
    std::vector<uint32_t> enumerate();       /* template ids on file */
    bool remove(uint32_t fid);

private:
    bool keymasterKey(std::vector<uint8_t> *key);
    void fillConfig(uint8_t *cfg) const;
    bool fail(const std::string &what);

    TeeClient m_tee;
    TeeShm m_req, m_rsp;
    uint32_t m_session = 0;
    bool m_haveSession = false;
    bool m_started = false;
    int m_chipId = 0;
    std::string m_error;
};

#endif
