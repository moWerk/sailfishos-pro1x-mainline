/*
 * SPDX-FileCopyrightText: 2025-2026 Jolla Mobile Ltd
 * SPDX-FileCopyrightText: 2026 moWerk (sysfs backend)
 * SPDX-License-Identifier: Apache-2.0
 *
 * droidmedia-flashlight with the Android camera service replaced by LED class
 * devices: the torch LEDs listed in /etc/sysfs-flashlight.conf are switched through
 * their sysfs brightness files.
 */

#include <QtDebug>
#include <QObject>
#include <QCoreApplication>
#include <QTimer>
#include <QList>

#include <policy/resource-set.h>
#include <policy/resources.h>
#include <QFile>
#include <QSettings>

#include "flashlightservice.h"

FlashlightService::FlashlightService(QObject *parent)
    : QObject(parent),
      m_resourceSet(nullptr),
      m_wantedState(false), m_flashlightAvailable(true),
      m_torchActive(false)
{
    QSettings conf(QStringLiteral("/etc/sysfs-flashlight.conf"), QSettings::IniFormat);
    for (const QString &led : conf.value(QStringLiteral("Torch/Leds")).toStringList())
        m_leds << QStringLiteral("/sys/class/leds/%1/brightness").arg(led.trimmed());
    m_brightness = conf.value(QStringLiteral("Torch/Brightness"), QStringLiteral("255")).toString();
    if (m_leds.isEmpty())
        qWarning() << "no Torch/Leds in /etc/sysfs-flashlight.conf";

    // Resource-policy reports availability asynchronously after D-Bus activation.
    // Allow the first request through and let acquire()/resourcesDenied() arbitrate.
    m_resourceSet = new ResourcePolicy::ResourceSet("background", this, true, true);
    m_resourceSet->addResource(ResourcePolicy::RearFlashlightType);
    connect(m_resourceSet, &ResourcePolicy::ResourceSet::resourcesBecameAvailable,
            this, &FlashlightService::resourcesAvailableHandler);
    connect(m_resourceSet, &ResourcePolicy::ResourceSet::resourcesGranted,
            this, &FlashlightService::resourcesAcquiredHandler);
    connect(m_resourceSet, &ResourcePolicy::ResourceSet::resourcesDenied,
            this, &FlashlightService::resourcesDeniedHandler);
    connect(m_resourceSet, &ResourcePolicy::ResourceSet::lostResources,
            this, &FlashlightService::resourcesLostHandler);
    m_resourceSet->initAndConnect();

    m_timer.setInterval(2 * 60 * 1000);
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &FlashlightService::deactivate);
    m_timer.start(); // just in case the user never turns the flashlight on.
}

FlashlightService::~FlashlightService()
{
    cleanup();
}

void FlashlightService::deactivate()
{
    bool needEmit = isFlashlightOn();
    cleanup();
    if (needEmit) {
        emit flashlightOnChanged(false);
    }
    qApp->quit();
}

bool FlashlightService::isFlashlightOn() const
{
    return m_torchActive;
}

void FlashlightService::cleanup()
{
    if (m_torchActive) {
        setLedTorch(false);
    }
}

bool FlashlightService::setLedTorch(bool active)
{
    bool ok = !m_leds.isEmpty();
    for (const QString &path : m_leds) {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly) ||
            f.write((active ? m_brightness : QStringLiteral("0")).toLatin1()) < 0) {
            qWarning() << "cannot write" << path << f.errorString();
            ok = false;
        }
    }

    if (!ok) {
        if (!active)
            m_torchActive = false;
        return false;
    }

    m_torchActive = active;
    return true;
}

bool FlashlightService::toggleFlashlight()
{
    if (!m_resourceSet)
        return false;

    // If someone else has acquired the flashlight resource do not even
    // try to turn the flashlight on.
    if (!m_flashlightAvailable)
        return false;

    m_wantedState = !m_wantedState;

    if (isFlashlightOn() == m_wantedState)
        return false;

    if (m_wantedState) {
        // Acquire flashlight resource, and if/when granted
        // set flaslight on.
        m_resourceSet->acquire();
    } else {
        setFlashlight(false);
        m_resourceSet->release();
    }

    return true;
}

void FlashlightService::setFlashlight(bool active)
{
    if (active) {
        if (isFlashlightOn()) {
            qWarning() << "Flashlight was already on.";
            return;
        }

        if (setLedTorch(true)) {
            m_timer.stop(); // stop the inactivity timer while the flashlight is on
            emit flashlightOnChanged(true);
            return;
        }

        qWarning() << "failed to turn on flashlight";
        m_wantedState = false;
        if (m_resourceSet)
            m_resourceSet->release();
    } else {
        // the flashlight is on.  turn it off by destroying the active backend.
        cleanup();
        m_timer.start(); // start the inactivity timer
        emit flashlightOnChanged(false);
    }
}

void FlashlightService::resourcesAvailableHandler(const QList<ResourcePolicy::ResourceType> &available)
{
    m_flashlightAvailable = available.contains(ResourcePolicy::RearFlashlightType);
}

void FlashlightService::resourcesAcquiredHandler()
{
    setFlashlight(m_wantedState);
}

void FlashlightService::resourcesDeniedHandler()
{
    m_wantedState = false;
}

void FlashlightService::resourcesLostHandler()
{
    m_wantedState = false;
    setFlashlight(false);
}
