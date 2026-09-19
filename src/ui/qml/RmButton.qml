import QtQuick

// A quiet control: raspberry only when armed, live or selected.
Rectangle {
    id: root
    property string text: ""
    property url iconSource: ""
    property bool primary: false
    property bool selected: false
    signal clicked()

    implicitWidth: Math.max(Theme.targetMin, row.implicitWidth + 2 * Theme.space6)
    implicitHeight: Theme.targetMin
    radius: Theme.radiusControl
    color: primary ? Theme.raspberry : (mouse.pressed || selected ? Theme.surfaceRaised : Theme.surface)
    border.width: selected ? 2 : 1
    border.color: primary ? Theme.raspberry : (selected ? Theme.milk : Theme.border)
    opacity: enabled ? 1 : 0.4

    Behavior on color {
        ColorAnimation { duration: Theme.durationFast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easingCurve }
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.space2
        AppImage {
            visible: root.iconSource != ""
            location: root.iconSource
            sourceSize: Qt.size(36, 36)
            width: 36
            height: 36
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: root.text !== ""
            text: root.text
            color: root.primary ? Theme.canvas : Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontControl
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
