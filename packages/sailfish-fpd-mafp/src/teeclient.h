// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * A thin client of the QSEECOM TEE driver (Bartosz Wróbel's out-of-tree
 * drivers/tee/qseecom) through the Linux TEE ioctls: find the device by its
 * implementation id, allocate shared memory, open a session to a trusted
 * application by name, send one command.
 *
 * The request is passed as an INOUT memref: the driver then copies the
 * staged request back after the send, which is how a trusted application
 * that answers in place (over its request) is read.
 */
#ifndef TEECLIENT_H
#define TEECLIENT_H

#include <cstddef>
#include <cstdint>

class TeeShm
{
public:
    TeeShm() = default;
    ~TeeShm();
    TeeShm(const TeeShm &) = delete;
    TeeShm &operator=(const TeeShm &) = delete;

    bool alloc(int devFd, size_t size);
    uint8_t *data() const { return m_va; }
    size_t size() const { return m_size; }
    int id() const { return m_id; }
    void clear();

private:
    uint8_t *m_va = nullptr;
    size_t m_size = 0;
    int m_id = -1;
};

class TeeClient
{
public:
    TeeClient() = default;
    ~TeeClient();
    TeeClient(const TeeClient &) = delete;
    TeeClient &operator=(const TeeClient &) = delete;

    /* Opens the first /dev/teeN whose implementation is QSEECOM. */
    bool open();
    void close();
    bool isOpen() const { return m_fd >= 0; }
    int fd() const { return m_fd; }

    /* A session to a loaded trusted application, by its name. */
    bool openSession(const char *app, uint32_t *session);
    void closeSession(uint32_t session);

    /*
     * One command: req (INOUT) and rsp (OUTPUT), both whole buffers.
     * Returns the TEE return code (0 on success) or -1 on an ioctl error
     * (errno set).
     */
    int invoke(uint32_t session, const TeeShm &req, size_t reqSize,
               const TeeShm &rsp, size_t rspSize);

private:
    int m_fd = -1;
};

#endif
