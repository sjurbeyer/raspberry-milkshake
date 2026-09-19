import QtQuick

// A modal question with cancel and confirm. Tapping outside the card cancels. Used before actions
// that end what is playing.
Item {
    id: root
    property string title: ""
    property string message: ""
    property string confirmText: "confirm"
    property string cancelText: "cancel"
    signal confirmed()
    signal cancelled()

    visible: false

    function open() {
        visible = true
        forceActiveFocus()
    }
    function close() {
        visible = false
    }
    function cancel() {
        close()
        cancelled()
    }
    function confirm() {
        close()
        confirmed()
    }

    Keys.onEscapePressed: cancel()

    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0.094, 0.094, 0.11, 0.82)  // the canvas colour, mostly opaque
    }
    MouseArea {
        objectName: "dialogScrim"
        anchors.fill: parent
        onClicked: root.cancel()
    }

    Rectangle {
        id: card
        objectName: "dialogCard"
        anchors.centerIn: parent
        width: Math.min(640, root.width - 2 * Theme.space6)
        height: column.height + 2 * Theme.space6
        radius: Theme.radiusPanel
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        MouseArea { anchors.fill: parent }  // taps on the card do not reach the scrim

        Column {
            id: column
            x: Theme.space6
            y: Theme.space6
            width: card.width - 2 * Theme.space6
            spacing: Theme.space4

            Text {
                objectName: "dialogTitle"
                width: parent.width
                text: root.title
                wrapMode: Text.WordWrap
                color: Theme.text
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
            }
            Text {
                objectName: "dialogMessage"
                width: parent.width
                text: root.message
                wrapMode: Text.WordWrap
                color: Theme.textMuted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontControl
            }
            Row {
                anchors.right: parent.right
                spacing: Theme.space4
                RmButton {
                    objectName: "dialogCancel"
                    text: root.cancelText
                    onClicked: root.cancel()
                }
                RmButton {
                    objectName: "dialogConfirm"
                    primary: true
                    text: root.confirmText
                    onClicked: root.confirm()
                }
            }
        }
    }
}
