set(SOURCES
    sensorlogd.cpp
    logger.cpp
    logger.h
    sensorPlugins/stepCounter.cpp
    sensorPlugins/stepCounter.h
    ../common.cpp
    ../common.h
    ../stepsource.h
)
if(SENSORLOGD_HEARTRATE)
    list(APPEND SOURCES sensorPlugins/heartrateSensor.cpp sensorPlugins/heartrateSensor.h)
endif()
if(SENSORLOGD_BAROMETER)
    list(APPEND SOURCES sensorPlugins/barometerSensor.cpp sensorPlugins/barometerSensor.h)
endif()
add_executable(sensorlogd ${SOURCES})
set_target_properties(sensorlogd PROPERTIES AUTOMOC ON)
target_link_libraries(sensorlogd PRIVATE
    Qt5::Core
    Qt5::DBus
    Qt5::Qml
    Qt5::Sensors
    ${mlite5_LIBRARIES}
    ${sensord_LIBRARIES}
)
target_include_directories(sensorlogd PRIVATE ${mlite5_INCLUDE_DIRS} ${sensord_INCLUDE_DIRS})
install(TARGETS sensorlogd DESTINATION bin)
install(FILES systemd/asteroid-sensorlogd.service
		DESTINATION /usr/lib/systemd/user)
