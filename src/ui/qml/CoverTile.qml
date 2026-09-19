import QtQuick

// One collection in the grid: cover, title (up to two lines) and type.
Item {
    id: root
    property string collectionId: ""
    property string title: ""
    property string collectionType: ""
    property url cover: ""
    property int coverSize: 208
    signal tapped()

    width: coverSize
    height: column.height

    scale: mouse.pressed ? 0.97 : 1.0
    Behavior on scale {
        NumberAnimation { duration: Theme.durationFast; easing.type: Easing.BezierSpline; easing.bezierCurve: Theme.easingCurve }
    }

    Column {
        id: column
        spacing: Theme.space2

        CoverArt {
            size: root.coverSize
            source: root.cover
        }
        Text {
            width: root.coverSize
            topPadding: Theme.space1
            text: root.title
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontControl
            font.weight: Font.Medium
        }
        Text {
            width: root.coverSize
            text: root.collectionType
            elide: Text.ElideRight
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMeta
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        onClicked: root.tapped()
    }
}
