// SPDX-License-Identifier: GPL-3.0-or-later
#include "teeclient.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <linux/tee.h>

#ifndef TEE_IMPL_ID_QSEECOM
#define TEE_IMPL_ID_QSEECOM 5
#endif

TeeShm::~TeeShm()
{
    if (m_va)
        munmap(m_va, m_size);
}

bool TeeShm::alloc(int devFd, size_t size)
{
    struct tee_ioctl_shm_alloc_data data;
    memset(&data, 0, sizeof(data));
    data.size = size;
    int shmFd = ioctl(devFd, TEE_IOC_SHM_ALLOC, &data);
    if (shmFd < 0)
        return false;
    void *va = mmap(nullptr, data.size, PROT_READ | PROT_WRITE, MAP_SHARED, shmFd, 0);
    ::close(shmFd);
    if (va == MAP_FAILED)
        return false;
    m_va = static_cast<uint8_t *>(va);
    m_size = data.size;
    m_id = data.id;
    clear();
    return true;
}

void TeeShm::clear()
{
    if (m_va)
        memset(m_va, 0, m_size);
}

TeeClient::~TeeClient()
{
    close();
}

bool TeeClient::open()
{
    for (int i = 0; i < 16; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/tee%d", i);
        int fd = ::open(path, O_RDWR);
        if (fd < 0)
            continue;
        struct tee_ioctl_version_data v;
        memset(&v, 0, sizeof(v));
        if (!ioctl(fd, TEE_IOC_VERSION, &v) && v.impl_id == TEE_IMPL_ID_QSEECOM) {
            m_fd = fd;
            return true;
        }
        ::close(fd);
    }
    errno = ENODEV;
    return false;
}

void TeeClient::close()
{
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

bool TeeClient::openSession(const char *app, uint32_t *session)
{
    /* The arg structs end in a flexible array of params: a byte buffer sized for both. */
    uint64_t storage[(sizeof(struct tee_ioctl_open_session_arg) + sizeof(struct tee_ioctl_param) + 7) / 8];
    memset(storage, 0, sizeof(storage));
    struct tee_ioctl_open_session_arg *arg = reinterpret_cast<struct tee_ioctl_open_session_arg *>(storage);
    struct tee_ioctl_param *params = arg->params;
    struct tee_ioctl_buf_data bd = { reinterpret_cast<uintptr_t>(storage), sizeof(storage) };

    TeeShm name;
    if (!name.alloc(m_fd, 64))
        return false;
    strncpy(reinterpret_cast<char *>(name.data()), app, 63);
    arg->num_params = 1;
    params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
    params[0].b = 64;
    params[0].c = name.id();
    if (ioctl(m_fd, TEE_IOC_OPEN_SESSION, &bd))
        return false;
    if (arg->ret) {
        errno = EIO;
        return false;
    }
    *session = arg->session;
    return true;
}

void TeeClient::closeSession(uint32_t session)
{
    struct tee_ioctl_close_session_arg arg = { session };
    ioctl(m_fd, TEE_IOC_CLOSE_SESSION, &arg);
}

int TeeClient::invoke(uint32_t session, const TeeShm &req, size_t reqSize,
                      const TeeShm &rsp, size_t rspSize)
{
    uint64_t storage[(sizeof(struct tee_ioctl_invoke_arg) + 2 * sizeof(struct tee_ioctl_param) + 7) / 8];
    memset(storage, 0, sizeof(storage));
    struct tee_ioctl_invoke_arg *arg = reinterpret_cast<struct tee_ioctl_invoke_arg *>(storage);
    struct tee_ioctl_param *params = arg->params;
    struct tee_ioctl_buf_data bd = { reinterpret_cast<uintptr_t>(storage), sizeof(storage) };

    arg->session = session;
    arg->num_params = 2;
    params[0].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT;
    params[0].b = reqSize;
    params[0].c = req.id();
    params[1].attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT;
    params[1].b = rspSize;
    params[1].c = rsp.id();
    if (ioctl(m_fd, TEE_IOC_INVOKE, &bd))
        return -1;
    return static_cast<int>(arg->ret);
}
