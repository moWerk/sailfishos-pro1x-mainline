add_library(
    sensorlogdqmlplugin
    sensorlogdqmlplugin.cpp
    sensorlogdqmlplugin.h
    stepsDataLoader.cpp
    stepsDataLoader.h
    hrDataLoader.cpp
    hrDataLoader.h
    weightDataLoader.cpp
    weightDataLoader.h
    barometerDataLoader.cpp
    barometerDataLoader.h
    loggerSettings.cpp
    loggerSettings.h
    ../common.cpp
    ../common.h
    ../stepsource.h
)
set_target_properties(sensorlogdqmlplugin PROPERTIES AUTOMOC ON)
target_include_directories(sensorlogdqmlplugin PRIVATE ${sensord_INCLUDE_DIRS})

target_link_libraries(
    sensorlogdqmlplugin
    Qt5::Qml
    Qt5::Quick
    Qt5::DBus
    Qt5::Sensors
    ${sensord_LIBRARIES}
)

install(TARGETS sensorlogdqmlplugin
	DESTINATION ${INSTALL_QML_IMPORT_DIR}/org/asteroid/sensorlogd)
install(FILES qmldir
	DESTINATION ${INSTALL_QML_IMPORT_DIR}/org/asteroid/sensorlogd)
