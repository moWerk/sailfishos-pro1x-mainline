// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * The sensor's power enable line (Pro1X: TLMM GPIO 108, "microarray,gpio_pwr"
 * in the stock device tree), held high through the GPIO character device for
 * as long as this object lives. The chip is found by its label so a change
 * in probe order does not point at a PMIC GPIO block.
 */
#ifndef SENSORPOWER_H
#define SENSORPOWER_H

#include <string>

class SensorPower
{
public:
    SensorPower() = default;
    ~SensorPower();
    SensorPower(const SensorPower &) = delete;
    SensorPower &operator=(const SensorPower &) = delete;

    /* chipLabel: the gpiochip label (Pro1X TLMM: "500000.pinctrl"). */
    bool hold(const std::string &chipLabel, unsigned int line, bool high);
    void release();
    bool held() const { return m_fd >= 0; }

private:
    int m_fd = -1;
};

#endif
