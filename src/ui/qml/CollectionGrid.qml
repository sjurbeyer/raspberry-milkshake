import QtQuick

// The scrollable grid of covers. Tapping one asks to open it in the swipe view.
GridView {
    id: grid
    signal opened(int row)

    readonly property int columns: 5
    cellWidth: width / columns
    cellHeight: 324
    topMargin: Theme.space2
    bottomMargin: Theme.space2
    clip: true
    cacheBuffer: cellHeight * 2
    boundsBehavior: Flickable.StopAtBounds

    delegate: Item {
        id: cell
        required property var model
        width: grid.cellWidth
        height: grid.cellHeight

        CoverTile {
            objectName: "tile-" + cell.model.collectionId
            anchors.horizontalCenter: parent.horizontalCenter
            collectionId: cell.model.collectionId
            title: cell.model.title
            collectionType: cell.model.collectionType
            cover: cell.model.cover
            onTapped: grid.opened(cell.model.index)
        }
    }
}
