import QtQuick

// The touch display's home screen (1280x720 landscape): a scrollable grid of collections, or one
// collection at a time to swipe through. The view button is at the top left, the filters at the
// top right.
Item {
    id: root
    required property var libraryFilter  // LibraryFilter
    required property var controller     // AppController
    property var tags: []
    property string view: "grid"         // "grid" or "swipe"
    property int previewDelay: 250
    property alias carousel: carouselView

    implicitWidth: 1280
    implicitHeight: 720

    function openSwipe(row) {
        carouselView.currentIndex = row
        view = "swipe"
    }
    function clearFilters() {
        libraryFilter.typeFilter = "all"
        libraryFilter.tagFilter = "all"
        typeDropdown.value = "all"
        tagDropdown.value = "all"
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    // Tapping anywhere else closes an open dropdown.
    MouseArea {
        anchors.fill: parent
        z: 5
        enabled: typeDropdown.open || tagDropdown.open
        onClicked: {
            typeDropdown.open = false
            tagDropdown.open = false
        }
    }

    Item {
        id: topBar
        z: 10
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: 88

        RmButton {
            id: toggle
            objectName: "viewToggle"
            x: Theme.space6
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.targetMin
            iconSource: root.view === "grid" ? "assets/carousel.svg" : "assets/grid.svg"
            onClicked: root.view = root.view === "grid" ? "swipe" : "grid"
        }
        Row {
            anchors.left: toggle.right
            anchors.leftMargin: Theme.space4
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space3
            AppImage {
                location: "assets/mark.svg"
                sourceSize: Qt.size(40, 40)
                width: 40
                height: 40
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: "raspberry milkshake"
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontControl
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Row {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space6
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space3

            RmDropdown {
                id: typeDropdown
                objectName: "typeDropdown"
                label: "type"
                options: [
                    { text: "all", value: "all" },
                    { text: "albums", value: "album" },
                    { text: "playlists", value: "playlist" },
                    { text: "interactive", value: "interactive" }
                ]
                onActivated: value => root.libraryFilter.typeFilter = value
                onOpenChanged: if (open) tagDropdown.open = false
            }
            RmDropdown {
                id: tagDropdown
                objectName: "tagDropdown"
                label: "tag"
                options: [{ text: "all", value: "all" }].concat(root.tags.map(t => ({ text: t, value: t })))
                onActivated: value => root.libraryFilter.tagFilter = value
                onOpenChanged: if (open) typeDropdown.open = false
            }
        }
    }

    Item {
        id: content
        anchors { top: topBar.bottom; left: parent.left; right: parent.right; bottom: playbackBar.top }

        CollectionGrid {
            id: gridView
            objectName: "grid"
            anchors.fill: parent
            visible: root.view === "grid" && root.libraryFilter.count > 0
            model: root.libraryFilter
            onOpened: row => root.openSwipe(row)
        }
        CollectionCarousel {
            id: carouselView
            objectName: "carouselView"
            anchors.fill: parent
            visible: root.view === "swipe" && root.libraryFilter.count > 0
            model: root.libraryFilter
            controller: root.controller
            previewDelay: root.previewDelay
        }
        Column {
            objectName: "emptyState"
            visible: root.libraryFilter.count === 0
            anchors.centerIn: parent
            spacing: Theme.space4
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "nothing matches these filters"
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontControl
            }
            RmButton {
                objectName: "clearFilters"
                anchors.horizontalCenter: parent.horizontalCenter
                text: "clear filters"
                onClicked: root.clearFilters()
            }
        }
    }

    PlaybackBar {
        id: playbackBar
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        controller: root.controller
        onStopRequested: stopDialog.open()
    }

    // Stopping ends what is playing and empties the queue, so it always asks first.
    ConfirmDialog {
        id: stopDialog
        objectName: "stopDialog"
        anchors.fill: parent
        z: 40
        title: "stop playback?"
        message: root.controller.queueLength > 0
                 ? "Stop " + root.controller.nowPlayingTitle + " and clear the queue?"
                 : "Stop " + root.controller.nowPlayingTitle + "?"
        confirmText: "stop"
        onConfirmed: root.controller.stopAndClear()
    }
    Connections {
        target: root.controller
        function onTransportChanged() {
            if (!root.controller.playing) stopDialog.close()  // it ended by itself while asking
        }
    }

    // The space bar pauses and continues (for the keyboard in jam mode and without a touch display).
    focus: true
    Keys.onSpacePressed: event => {
        if (root.controller.playing && !stopDialog.visible) root.controller.togglePause()
        event.accepted = true
    }
    Component.onCompleted: forceActiveFocus()
}
