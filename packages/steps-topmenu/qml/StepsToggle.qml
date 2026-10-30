import QtQuick 2.6
import Sailfish.Silica 1.0
import com.jolla.settings 1.0
import org.asteroid.sensorlogd 1.0

// Today's steps in the top menu. The count comes from sensorlogd's QML module (the step logger
// by Arseniy Movshev / dodoradio), which reads sensorfw's step counter; on the Pro1-X the
// Qualcomm Sensor Core counts the steps, also while the phone sleeps.
SettingsToggle {
    id: root

    name: "Steps"
    activeText: Number(steps.todayTotal).toLocaleString(Qt.locale(), "f", 0) + " steps"
    active: true
    showOnOffLabel: false
    icon.source: "image://theme/icon-m-health"

    // A tap asks the daemon to write a record now, so the day's log is current
    onToggled: steps.triggerDaemonRecording()

    StepsDataLoader {
        id: steps
    }
}
