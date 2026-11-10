TARGET = sailfish-fpd-mafp
TEMPLATE = app
QT -= gui
QT += dbus
CONFIG += c++11
# Linux UAPI headers newer than the SDK target's kernel-headers (tee, gpio v2)
INCLUDEPATH += uapi
LIBS += -lpthread

SOURCES += src/main.cpp \
    src/fpd.cpp \
    src/mafpbackend.cpp \
    src/mafpta.cpp \
    src/teeclient.cpp \
    src/sensorpower.cpp

HEADERS += src/fpd.h \
    src/mafpbackend.h \
    src/mafpta.h \
    src/teeclient.h \
    src/sensorpower.h

target.path = /usr/bin

dbus.files = org.sailfishos.fingerprint1.conf
dbus.path = /etc/dbus-1/system.d

unit.files = sailfish-fpd-mafp.service
unit.path = /usr/lib/systemd/system

INSTALLS += target dbus unit
