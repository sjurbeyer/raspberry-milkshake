import QtQuick

// A labelled dropdown. `options` is a list of { text, value }.
Item {
    id: root
    property string label: ""
    property var options: []
    property string value: options.length > 0 ? options[0].value : ""
    property bool open: false
    signal activated(string value)

    implicitWidth: 250
    implicitHeight: Theme.targetMin

    function textFor(v) {
        for (let i = 0; i < options.length; ++i) {
            if (options[i].value === v) return options[i].text
        }
        return v
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusControl
        color: root.open || mouse.pressed ? Theme.surfaceRaised : Theme.surface
        border.width: root.open ? 2 : 1
        border.color: root.open ? Theme.milk : Theme.border

        Row {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Theme.space4
            spacing: Theme.space2
            Text {
                text: root.label
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontMeta
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.textFor(root.value)
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontControl
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        AppImage {
            location: "assets/chevron-down.svg"
            sourceSize: Qt.size(36, 36)
            width: 36
            height: 36
            anchors.right: parent.right
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            rotation: root.open ? 180 : 0
            Behavior on rotation {
                NumberAnimation { duration: Theme.durationFast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easingCurve }
            }
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            onClicked: root.open = !root.open
        }
    }

    Rectangle {
        id: popup
        visible: root.open
        z: 100
        y: root.height + Theme.space1
        width: Math.max(root.width, 260)
        height: column.height + 2
        radius: Theme.radiusPanel
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        Column {
            id: column
            x: 1
            y: 1
            width: parent.width - 2
            Repeater {
                model: root.options
                delegate: Rectangle {
                    required property var modelData
                    objectName: root.objectName + "-option-" + modelData.value
                    width: column.width
                    height: Theme.targetMin
                    color: optionMouse.pressed ? Theme.surfaceRaised : "transparent"
                    Rectangle {
                        visible: modelData.value === root.value
                        width: 10
                        height: 10
                        radius: 5
                        color: Theme.raspberry
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.space4
                    }
                    Text {
                        text: modelData.text
                        color: modelData.value === root.value ? Theme.text : Theme.textMuted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontControl
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.space4 + 26
                    }
                    MouseArea {
                        id: optionMouse
                        anchors.fill: parent
                        onClicked: {
                            root.value = modelData.value
                            root.open = false
                            root.activated(modelData.value)
                        }
                    }
                }
            }
        }
    }
}
