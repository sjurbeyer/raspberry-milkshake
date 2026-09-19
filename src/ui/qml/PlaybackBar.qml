import QtQuick

// Shown while something plays or is paused: what it is, how many are queued, and the transport
// controls: pause or continue, next, and stop (which the home screen confirms first).
Rectangle {
    id: root
    required property var controller  // AppController
    signal stopRequested()
    objectName: "playbackBar"
    height: controller.playing ? Theme.targetMin : 0
    visible: controller.playing
    color: Theme.surface

    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        id: nowPlaying
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Theme.space6
        spacing: Theme.space3

        LiveDot {
            visible: !root.controller.paused
            anchors.verticalCenter: parent.verticalCenter
        }
        Row {  // a small pause mark instead of the live dot
            objectName: "pausedMark"
            visible: root.controller.paused
            spacing: 4
            anchors.verticalCenter: parent.verticalCenter
            Repeater {
                model: 2
                Rectangle { width: 5; height: 16; radius: 1; color: Theme.textMuted }
            }
        }
        Text {
            objectName: "transportLabel"
            text: root.controller.paused ? "paused" : "now playing"
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMeta
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            objectName: "nowPlayingTitle"
            width: Math.min(implicitWidth, root.width - 760)
            elide: Text.ElideRight
            text: root.controller.nowPlayingTitle
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontControl
            font.weight: Font.Medium
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: Theme.space6
        spacing: Theme.space3
        Text {
            objectName: "queueLabel"
            text: "queue: " + root.controller.queueLength
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMeta
            anchors.verticalCenter: parent.verticalCenter
        }
        RmButton {
            objectName: "pauseButton"
            text: root.controller.paused ? "continue" : "pause"
            primary: root.controller.paused
            height: Theme.targetMin - Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.controller.togglePause()
        }
        RmButton {
            objectName: "nextButton"
            text: "next"
            height: Theme.targetMin - Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.controller.next()
        }
        RmButton {
            objectName: "stopButton"
            text: "stop"
            height: Theme.targetMin - Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.stopRequested()
        }
    }
}
