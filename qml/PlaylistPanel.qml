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
            if (event.key === Qt.Key_Q) {
                if (list.currentItem) {
                    pl.toggleQueue(list.currentItem.originalIndex)
                } else if (playlistModel.currentRow >= 0) {
                    pl.toggleQueue(playlistModel.currentRow)
                }
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
            required property string duration
            required property int queue

            width: list.width
            highlighted: row.originalIndex === playlistModel.currentRow
            leftPadding: 6
            rightPadding: 6
            topPadding: 6
            bottomPadding: 6

            background: Rectangle {
                radius: 3
                color: row.highlighted ? row.palette.highlight : "transparent"
            }

            contentItem: Item {
                implicitWidth: durationText.implicitWidth + nameText.implicitWidth + 8
                implicitHeight: Math.max(durationText.implicitHeight, nameText.implicitHeight)

                Label {
                    id: durationText
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: "monospace"
                    text: row.duration
                    color: row.highlighted ? row.palette.highlightedText : row.palette.text
                }
                Label {
                    id: nameText
                    anchors.left: parent.left
                    anchors.right: durationText.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: row.queue > 0 ? "[" + row.queue + "] " + row.name : row.name
                    color: row.highlighted ? row.palette.highlightedText : row.palette.text
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
            icon.name: "document-close"
            Component.onCompleted: text = pl.removeText
            onTriggered: pl.remove(rowMenu.targetRow)
        }
    }
}
