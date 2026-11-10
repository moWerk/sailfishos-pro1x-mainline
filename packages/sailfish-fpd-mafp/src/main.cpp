// SPDX-License-Identifier: GPL-3.0-or-later
#include "fpd.h"

#include <QCoreApplication>

#include <csignal>
#include <cstdio>

static void onSignal(int)
{
    QCoreApplication::quit();
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("sailfishos"));
    QCoreApplication::setApplicationName(QStringLiteral("sailfish-fpd-mafp"));

    setlinebuf(stdout);
    setlinebuf(stderr);
    signal(SIGTERM, onSignal);
    signal(SIGINT, onSignal);

    qRegisterMetaType<uint32_t>("uint32_t");

    Fpd service;
    return app.exec();
}
