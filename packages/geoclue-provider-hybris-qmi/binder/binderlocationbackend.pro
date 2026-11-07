include($$PWD/../geoclue-providers-hybris.pri)

HEADERS += \
    binderlocationbackend.h \
    binderlocationbackend_aidl.h \
    binderlocationbackend_hidl.h

SOURCES += \
    binderlocationbackend.cpp \
    binderlocationbackend_aidl.cpp \
    binderlocationbackend_hidl.cpp

PKGCONFIG += libgbinder libglibutil gobject-2.0 glib-2.0
