cmake_minimum_required(VERSION 3.10.0)
project(sensorlogd
    VERSION 1.0.0
    DESCRIPTION "A sensor logger daemon for AsteroidOS oriented towards health tracking"
)

find_package(PkgConfig)

include(FeatureSummary)
include(GNUInstallDirs)

# AsteroidOS builds everything; SailfishOS has no heart rate sensor class in QtSensors 5.6 and
# no step counter there either, so it reads steps from sensorfw (SENSORLOGD_SENSORFW).
option(SENSORLOGD_HEARTRATE "Log the heart rate (QtSensors QHrmSensor)" ON)
option(SENSORLOGD_BAROMETER "Log the barometer (QtSensors QPressureSensor, mlite5)" ON)
option(SENSORLOGD_SENSORFW "Read the step counter from sensorfw instead of QtSensors" OFF)
option(SENSORLOGD_COUNTER_RESETS_ON_OPEN "The step counter starts from 0 when opened, not at boot" OFF)
set(INSTALL_QML_IMPORT_DIR "${CMAKE_INSTALL_LIBDIR}/qt5/qml" CACHE PATH "QML import directory")

find_package(Qt5 COMPONENTS Core DBus Qml Quick Sensors REQUIRED)
if(SENSORLOGD_BAROMETER)
    pkg_check_modules(mlite5 REQUIRED mlite5)
endif()
if(SENSORLOGD_SENSORFW)
    pkg_check_modules(sensord REQUIRED sensord-qt5)
endif()
foreach(opt SENSORLOGD_HEARTRATE SENSORLOGD_BAROMETER SENSORLOGD_SENSORFW SENSORLOGD_COUNTER_RESETS_ON_OPEN)
    if(${opt})
        add_definitions(-D${opt})
    endif()
endforeach()

add_subdirectory(daemon)
add_subdirectory(qmlplugin)
