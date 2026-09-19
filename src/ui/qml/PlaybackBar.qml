import QtQuick

// Shown while something plays: what it is, how many are queued, and a way to skip ahead.
Rectangle {
    id: root
    required property var controller  // AppController
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
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: Theme.space6
        spacing: Theme.space3
        LiveDot { anchors.verticalCenter: parent.verticalCenter }
        Text {
            text: "now playing"
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMeta
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            objectName: "nowPlayingTitle"
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
        spacing: Theme.space4
        Text {
            objectName: "queueLabel"
            text: "queue: " + root.controller.queueLength
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMeta
            anchors.verticalCenter: parent.verticalCenter
        }
        RmButton {
            objectName: "nextButton"
            text: "next"
            height: Theme.targetMin - Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            onClicked: root.controller.next()
        }
    }
}
