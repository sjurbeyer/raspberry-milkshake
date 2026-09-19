import QtQuick

// The small raspberry live dot: breathes slowly unless reduced motion is on.
Rectangle {
    id: dot
    width: 14
    height: 14
    radius: width / 2
    color: Theme.raspberry

    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 12
        height: width
        radius: width / 2
        color: "transparent"
        border.width: 3
        border.color: Qt.rgba(0.91, 0.31, 0.55, 0.2)
    }

    SequentialAnimation on scale {
        running: !Theme.reducedMotion && dot.visible
        loops: Animation.Infinite
        NumberAnimation { to: 1.35; duration: 800; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easingCurve }
        NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easingCurve }
    }
}
