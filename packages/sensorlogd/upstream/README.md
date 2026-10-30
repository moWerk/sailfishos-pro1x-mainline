# Asteroid-sensorlogd
asteroid-sensorlogd is an extensible sensor logger which is intended for health tracking, which depends on nemo's MCE and Asteroid's QtSensors which was modified to add heartrate capability.
The project comes as two parts: the sensor logging daemon, which runs in the background to trigger sensor recordings, and the qml module which provides easy access to it. There is currently no facility for the data to be accessible to other languages.
## Recording triggers and power management
Sensorlogd will only record data when the CPU is already running. It does not schedule any wakeups, so if your device isn't woken up already, there will be no recordings. This is intended as a crude form of wear detection and as a power-saving measure. Better solutions can probably be implemented.

When sensorlogd receives the `displayOn` signal from MCE, it assumes this means 'the system has come out of suspend, so the system clock has significantly checked since the last time the daemon was aware of the time'. Each sensor plugin will then check if enough time has elapsed between readings.
## File format
For each given sensor, sensorlogd will create log files named `[year]-[month]-[day].log`, eg. `2023-03-12.log`. Each day a new file will be created. This structure is intended to make it fast to retrieve data for a single day with minimal seeking.
