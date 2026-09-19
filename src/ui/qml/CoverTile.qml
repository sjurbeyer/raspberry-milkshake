import QtQuick

// One collection in the grid: cover, title and type.
Item {
    id: root
    property string collectionId: ""
    property string title: ""
    property string collectionType: ""
    property url cover: ""
    property int coverSize: 208
    signal tapped()

    width: coverSize
    height: coverSize + 84

    scale: mouse.pressed ? 0.97 : 1.0
    Behavior on scale {
        NumberAnimation { duration: Theme.durationFast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easingCurve }
    }

    CoverArt {
        id: art
        size: root.coverSize
        source: root.cover
    }
    Text {
        anchors.top: art.bottom
        anchors.topMargin: Theme.space3
        width: root.coverSize
        text: root.title
        elide: Text.ElideRight
        color: Theme.text
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontControl
        font.weight: Font.Medium
    }
    Text {
        anchors.top: art.bottom
        anchors.topMargin: Theme.space3 + 32
        width: root.coverSize
        text: root.collectionType
        elide: Text.ElideRight
        color: Theme.textMuted
        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontMeta
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.tapped()
    }
}
