import QtQuick

// The scrollable grid of covers. Tapping one asks to open it in the swipe view.
GridView {
    id: grid
    signal opened(int row)

    readonly property int columns: 5
    cellWidth: 232
    cellHeight: 300
    leftMargin: Math.max(0, (width - columns * cellWidth) / 2)
    topMargin: Theme.space4
    bottomMargin: Theme.space4
    clip: true
    cacheBuffer: cellHeight * 2
    boundsBehavior: Flickable.StopAtBounds

    delegate: CoverTile {
        required property int index
        required property string collectionId
        required property string title
        required property string collectionType
        required property url cover
        objectName: "tile-" + collectionId
        onTapped: grid.opened(index)
    }
}
