import QtQuick

// One collection at a time, cover centred, swipe left or right for the next. While nothing is
// playing the centred collection is previewed on the main output; while something plays the
// preview is off and the choice is "add to queue" or "play now".
Item {
    id: root
    required property var model       // LibraryFilter
    required property var controller  // AppController
    property int previewDelay: 250    // ms of stillness before a preview starts
    property alias currentIndex: list.currentIndex
    property alias list: list

    readonly property int coverSize: Math.min(340, Math.max(160, height - 250))
    readonly property string currentId:
        (list.currentIndex >= 0 && list.currentIndex < model.count) ? model.idAt(list.currentIndex) : ""
    property var current: ({})
    property var actions: ({ previewAllowed: false, addToQueue: false, playNow: false })
    property bool previewing: false

    function refresh() {
        current = currentId === "" ? ({}) : model.get(list.currentIndex)
        actions = currentId === "" ? { previewAllowed: false, addToQueue: false, playNow: false }
                                   : controller.browseActions(currentId)
    }
    function updatePreview() {
        if (!visible || currentId === "") controller.stopBrowsing()
        else controller.browsing(currentId)
    }
    function metaText(c) {
        if (!c.collectionType) return ""
        const parts = [c.collectionType]
        if (c.collectionType === "interactive") parts.push("live audio")
        else parts.push(c.trackCount + (c.trackCount === 1 ? " track" : " tracks"))
        if (c.tags && c.tags.length > 0) parts.push(c.tags.join(", "))
        return parts.join(" · ")
    }

    onCurrentIdChanged: {
        refresh()
        previewing = false
        previewTimer.restart()
    }
    onVisibleChanged: {
        refresh()
        if (visible) previewTimer.restart()
        else controller.stopBrowsing()
    }
    Component.onCompleted: refresh()

    Timer {
        id: previewTimer
        interval: root.previewDelay
        onTriggered: root.updatePreview()
    }
    Connections {
        target: root.controller
        function onTransportChanged() { root.refresh(); previewTimer.restart() }
        function onQueueChanged() { root.refresh() }
        function onPreviewRequested(id) { root.previewing = (id === root.currentId) }
        function onPreviewStopped() { root.previewing = false }
    }
    Connections {
        target: root.model
        function onCountChanged() { root.refresh() }
    }

    ListView {
        id: list
        objectName: "carousel"
        anchors { top: parent.top; left: parent.left; right: parent.right }
        height: root.coverSize + 40
        orientation: ListView.Horizontal
        model: root.model
        spacing: 56
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        preferredHighlightBegin: (width - root.coverSize) / 2
        preferredHighlightEnd: (width + root.coverSize) / 2
        highlightMoveDuration: Theme.durationBase
        flickDeceleration: 4000
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: root.coverSize * 3

        delegate: Item {
            id: cell
            required property int index
            required property string collectionId
            required property url cover
            objectName: "slide-" + collectionId
            width: root.coverSize
            height: list.height

            readonly property real distance:
                Math.min(1, Math.abs(x + width / 2 - list.contentX - list.width / 2) / (width + list.spacing))
            scale: 1 - 0.16 * distance
            opacity: 1 - 0.55 * distance

            CoverArt {
                anchors.centerIn: parent
                size: root.coverSize
                source: cell.cover
            }
            MouseArea {
                anchors.fill: parent
                onClicked: list.currentIndex = cell.index
            }
        }
    }

    Column {
        id: info
        anchors { top: list.bottom; horizontalCenter: parent.horizontalCenter }
        width: parent.width
        spacing: Theme.space2

        Text {
            objectName: "currentTitle"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.current.title || ""
            elide: Text.ElideRight
            color: Theme.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontTitle
            font.weight: Font.DemiBold
        }
        Text {
            objectName: "currentMeta"
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: root.metaText(root.current)
            color: Theme.textMuted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMeta
        }
        Item {
            width: parent.width
            height: 24
            Row {
                objectName: "previewIndicator"
                visible: root.previewing
                anchors.centerIn: parent
                spacing: Theme.space3
                LiveDot { anchors.verticalCenter: parent.verticalCenter }
                Text {
                    text: "previewing on the main output"
                    color: Theme.textMuted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMeta
                }
            }
        }
        Row {
            objectName: "actionRow"
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.space4
            RmButton {
                objectName: "addToQueueButton"
                visible: root.actions.addToQueue
                text: "add to queue"
                onClicked: root.controller.addToQueue(root.currentId)
            }
            RmButton {
                objectName: "playNowButton"
                visible: root.actions.playNow && root.controller.playing
                primary: true
                text: "play now"
                onClicked: root.controller.playNow(root.currentId)
            }
            RmButton {
                objectName: "playButton"
                visible: root.actions.playNow && !root.controller.playing
                primary: true
                text: "play"
                onClicked: root.controller.playNow(root.currentId)
            }
        }
    }
}
