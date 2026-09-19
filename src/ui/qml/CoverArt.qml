import QtQuick

// A square cover. Collections without a cover file get the brand mark on a quiet tile.
Item {
    id: root
    property url source: ""
    property int size: 208
    width: size
    height: size

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceRaised
        AppImage {
            visible: root.source == ""
            anchors.centerIn: parent
            location: "assets/mark.svg"
            sourceSize: Qt.size(root.size * 0.5, root.size * 0.5)
            width: root.size * 0.5
            height: root.size * 0.5
            opacity: 0.6
        }
    }
    AppImage {
        anchors.fill: parent
        visible: root.source != ""
        location: root.source
        sourceSize: Qt.size(root.size, root.size)
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
    }
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Theme.border
    }
}
