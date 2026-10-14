import QtQuick
import QtQuick.Controls

Pane {
    id: panel

    implicitHeight: 160
    padding: 3
    background: Rectangle {
        radius: 6
        color: panel.palette.base
        border.color: panel.palette.mid
        border.width: 1
    }

    contentItem: ListView {
        id: list
        clip: true
        focus: true
        model: playlistModel
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        Keys.onPressed: function (event) {
            const isQueueKey = (event.key === Qt.Key_R || event.key === Qt.Key_Q ||
                                event.text === "r" || event.text === "R" ||
                                event.text === "к" || event.text === "К")
            if (isQueueKey) {
                if (list.currentItem && list.currentItem.originalIndex !== undefined) {
                    pl.toggleQueue(list.currentItem.originalIndex)
                } else if (list.currentIndex >= 0) {
                    const orig = playlistModel.originalIndex(list.currentIndex)
                    if (orig >= 0) {
                        pl.toggleQueue(orig)
                    } else if (playlistModel.currentRow >= 0) {
                        pl.toggleQueue(playlistModel.currentRow)
                    }
                } else if (playlistModel.currentRow >= 0) {
                    pl.toggleQueue(playlistModel.currentRow)
                }
                event.accepted = true
                return
            } else if (event.key === Qt.Key_Up) {
                if (list.currentIndex > 0) {
                    list.currentIndex--
                    if (list.currentItem) pl.select(list.currentItem.originalIndex)
                }
            } else if (event.key === Qt.Key_Down) {
                if (list.currentIndex < list.count - 1) {
                    list.currentIndex++
                    if (list.currentItem) pl.select(list.currentItem.originalIndex)
                }
            } else {
                return
            }
            if (list.currentIndex >= 0)
                list.positionViewAtIndex(list.currentIndex, ListView.Contain)
            event.accepted = true
        }

        Label {
            anchors.centerIn: parent
            visible: list.count === 0 && playlistModel && playlistModel.filter && playlistModel.filter.trim().length > 0
            text: qsTranslate("Kalorite::MainWindow", "No tracks found")
            color: panel.palette.placeholderText
            font.italic: true
        }

        delegate: ItemDelegate {
            id: row

            required property int index
            required property int originalIndex
            required property string name
            required property string title
            required property string artist
            required property string duration
            required property int queue
            required property bool hasCover
            required property string coverUrl

            width: list.width
            highlighted: row.originalIndex === playlistModel.currentRow
            leftPadding: 6
            rightPadding: 6
            topPadding: 4
            bottomPadding: 4

            background: Rectangle {
                radius: 4
                color: row.highlighted ? row.palette.highlight : "transparent"
            }

            contentItem: Item {
                implicitHeight: 40

                Rectangle {
                    id: coverContainer
                    width: 36
                    height: 36
                    radius: 6
                    clip: true
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    color: row.highlighted ? Qt.rgba(1, 1, 1, 0.15) : Qt.rgba(0, 0, 0, 0.15)
                    border.width: 1
                    border.color: row.highlighted ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(0, 0, 0, 0.08)

                    Image {
                        id: coverImg
                        anchors.fill: parent
                        source: row.hasCover ? row.coverUrl : ""
                        sourceSize: Qt.size(72, 72)
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        visible: row.hasCover && status === Image.Ready
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !coverImg.visible
                        text: "♪"
                        font.pixelSize: 18
                        font.bold: true
                        color: row.highlighted ? row.palette.highlightedText : row.palette.text
                        opacity: 0.4
                    }
                }

                Label {
                    id: durationText
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: "monospace"
                    font.pixelSize: 11
                    text: row.duration
                    color: row.highlighted ? row.palette.highlightedText : row.palette.text
                }

                Column {
                    anchors.left: coverContainer.right
                    anchors.leftMargin: 8
                    anchors.right: durationText.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Label {
                        id: titleText
                        width: parent.width
                        elide: Text.ElideRight
                        font.bold: true
                        font.pixelSize: 12
                        text: row.queue > 0 ? "[" + row.queue + "] " + (row.title ? row.title : row.name) : (row.title ? row.title : row.name)
                        color: row.highlighted ? row.palette.highlightedText : row.palette.text
                    }

                    Label {
                        id: artistText
                        width: parent.width
                        elide: Text.ElideRight
                        font.pixelSize: 10
                        text: row.artist && row.artist.trim().length > 0 ? row.artist : qsTranslate("Kalorite::MainWindow", "Unknown Artist")
                        color: row.highlighted ? row.palette.highlightedText : row.palette.placeholderText
                        opacity: row.highlighted ? 0.9 : 0.8
                    }
                }
            }

            // Drag-reorder: the row follows the pointer; the model move keeps the delegate alive.
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                preventStealing: true

                onPressed: (mouse) => {
                    if (mouse.button === Qt.RightButton) {
                        const p = mapToItem(panel, mouse.x, mouse.y)
                        rowMenu.targetRow = row.originalIndex
                        rowMenu.x = p.x
                        rowMenu.y = p.y
                        rowMenu.open()
                        return
                    }
                    pl.select(row.originalIndex)
                    list.currentIndex = row.index
                    list.forceActiveFocus()
                }

                onPositionChanged: (mouse) => {
                    if (playlistModel.filter && playlistModel.filter.trim().length > 0) return
                    if (!(mouse.buttons & Qt.LeftButton)) return
                    const p = mapToItem(list.contentItem, mouse.x, mouse.y)
                    const target = list.indexAt(p.x, p.y)
                    if (target >= 0 && target !== row.index)
                        playlistModel.moveTrack(row.index, target)
                }

                onDoubleClicked: (mouse) => {
                    if (mouse.button === Qt.LeftButton) pl.activate(row.originalIndex)
                }
            }
        }
    }

    Menu {
        id: rowMenu
        property int targetRow: -1

        MenuItem {
            text: qsTranslate("Kalorite::MainWindow", "Queue track") + " (R)"
            icon.name: "media-playlist-queue"
            onTriggered: pl.toggleQueue(rowMenu.targetRow)
        }

        MenuItem {
            icon.name: "document-close"
            Component.onCompleted: text = pl.removeText
            onTriggered: pl.remove(rowMenu.targetRow)
        }
    }
}
