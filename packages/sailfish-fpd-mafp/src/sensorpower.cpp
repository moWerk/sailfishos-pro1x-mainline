// SPDX-License-Identifier: GPL-3.0-or-later
#include "sensorpower.h"

#include <cstring>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/gpio.h>

SensorPower::~SensorPower()
{
    release();
}

bool SensorPower::hold(const std::string &chipLabel, unsigned int line, bool high)
{
    release();
    for (int i = 0; i < 16; i++) {
        char path[32];
        snprintf(path, sizeof(path), "/dev/gpiochip%d", i);
        int fd = open(path, O_RDWR);
        if (fd < 0)
            continue;
        struct gpiochip_info info;
        memset(&info, 0, sizeof(info));
        if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) || chipLabel != info.label ||
            line >= info.lines) {
            close(fd);
            continue;
        }
        struct gpio_v2_line_request req;
        memset(&req, 0, sizeof(req));
        req.offsets[0] = line;
        req.num_lines = 1;
        strncpy(req.consumer, "sailfish-fpd-mafp", sizeof(req.consumer) - 1);
        req.config.flags = GPIO_V2_LINE_FLAG_OUTPUT;
        req.config.num_attrs = 1;
        req.config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_OUTPUT_VALUES;
        req.config.attrs[0].attr.values = high ? 1 : 0;
        req.config.attrs[0].mask = 1;
        int ret = ioctl(fd, GPIO_V2_GET_LINE_IOCTL, &req);
        close(fd);
        if (ret)
            return false;
        m_fd = req.fd;
        return true;
    }
    return false;
}

void SensorPower::release()
{
    if (m_fd >= 0)
        close(m_fd);
    m_fd = -1;
}
