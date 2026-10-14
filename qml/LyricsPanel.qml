import QtQuick
import QtQuick.Controls

Item {
    id: lyricsPanel

    implicitWidth: 0
    implicitHeight: 160

    property bool autoScrollEnabled: true

    Rectangle {
        id: bgRect
        anchors.fill: parent
        radius: 6
        color: lyrics ? lyrics.backgroundColor : "#18181c"
        border.color: Qt.rgba(1, 1, 1, 0.08)
        border.width: 1

        Behavior on color {
            ColorAnimation { duration: 400 }
        }
    }

    NumberAnimation {
        id: scrollAnim
        target: list
        property: "contentY"
        duration: 400
        easing.type: Easing.OutCubic
    }

    function smoothScrollTo(idx) {
        if (!autoScrollEnabled || idx < 0 || list.moving) return
        list.currentIndex = idx

        let item = list.itemAtIndex(idx)
        if (item) {
            // Exactly center the item vertically in the viewport
            let targetY = item.y - Math.round((list.height - item.height) / 2)
            let minY = -list.topMargin
            let maxY = Math.max(minY, list.contentHeight - list.height + list.bottomMargin)
            let clampedY = Math.max(minY, Math.min(targetY, maxY))

            scrollAnim.stop()
            scrollAnim.from = list.contentY
            scrollAnim.to = clampedY
            scrollAnim.start()
        } else {
            list.positionViewAtIndex(idx, ListView.Center)
        }
    }

    // Main content area
    Item {
        anchors.fill: parent
        anchors.margins: 4
        clip: true

        // Loading state
        Label {
            anchors.centerIn: parent
            visible: lyrics && lyrics.isLoading
            text: qsTranslate("Kalorite::MainWindow", "Looking for lyrics...")
            color: "#ffffff"
            opacity: 0.6
            font.bold: true
            font.pixelSize: 13
        }

        // Empty / Not found state
        Column {
            anchors.centerIn: parent
            visible: lyrics && !lyrics.isLoading && !lyrics.hasLyrics
            spacing: 8

            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTranslate("Kalorite::MainWindow", "No lyrics available")
                color: "#ffffff"
                opacity: 0.8
                font.bold: true
                font.pixelSize: 13
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 6

                Button {
                    implicitHeight: 24
                    icon.name: "document-open"
                    text: qsTranslate("Kalorite::MainWindow", "Open .lrc")
                    font.pixelSize: 11
                    onClicked: if (lyrics) lyrics.openLrcFile()
                }

                Button {
                    implicitHeight: 24
                    icon.name: "view-refresh"
                    text: qsTranslate("Kalorite::MainWindow", "Retry")
                    font.pixelSize: 11
                    onClicked: if (lyrics) lyrics.reload()
                }
            }
        }

        // Spotify-style lyrics list view
        ListView {
            id: list
            anchors.fill: parent
            visible: lyrics && lyrics.hasLyrics && !lyrics.isLoading
            clip: true
            focus: true
            model: lyrics
            spacing: 6
            topMargin: Math.round(height / 2)
            bottomMargin: Math.round(height / 2)
            cacheBuffer: 3000
            boundsBehavior: Flickable.StopAtBounds

            onMovingChanged: {
                if (moving) {
                    scrollAnim.stop()
                    lyricsPanel.autoScrollEnabled = false
                    resumeScrollTimer.restart()
                }
            }

            Timer {
                id: resumeScrollTimer
                interval: 3500
                repeat: false
                onTriggered: {
                    lyricsPanel.autoScrollEnabled = true
                    if (lyrics && lyrics.activeIndex >= 0) {
                        lyricsPanel.smoothScrollTo(lyrics.activeIndex)
                    }
                }
            }

            Connections {
                target: lyrics
                function onActiveIndexChanged(idx) {
                    if (idx >= 0 && lyricsPanel.autoScrollEnabled && !list.moving) {
                        lyricsPanel.smoothScrollTo(idx)
                    }
                }
            }

            delegate: Item {
                id: lineRow

                required property int index
                required property string text
                required property int timeMs
                required property string timeStr
                required property bool isActive

                width: list.width
                height: lineText.implicitHeight + 10

                property bool isCurrent: lyrics.isSynced && lineRow.isActive
                property bool hovered: rowMouse.containsMouse

                Label {
                    id: lineText
                    anchors.centerIn: parent
                    width: parent.width - 32
                    text: lineRow.text.length > 0 ? lineRow.text : "♪"
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.weight: lineRow.isCurrent ? Font.Bold : Font.DemiBold
                    font.pixelSize: 18
                    renderType: Text.QtRendering
                    color: "#ffffff"
                    opacity: lineRow.isCurrent ? 1.0 : (lineRow.hovered ? 0.70 : 0.35)
                    scale: lineRow.isCurrent ? 1.0 : 0.72
                    transformOrigin: Item.Center

                    Behavior on opacity {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }
                    Behavior on scale {
                        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if (lyrics) {
                            lyrics.seekToLine(lineRow.index)
                            lyricsPanel.autoScrollEnabled = true
                            lyricsPanel.smoothScrollTo(lineRow.index)
                        }
                    }
                }
            }
        }

        // Floating action buttons (XDG icons, visible on hover in top right)
        Row {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 4
            spacing: 4
            opacity: controlsMouse.containsMouse ? 0.90 : 0.0

            Behavior on opacity { NumberAnimation { duration: 150 } }

            ToolButton {
                implicitWidth: 26
                implicitHeight: 26
                icon.name: "document-open"
                ToolTip.visible: hovered
                ToolTip.text: qsTranslate("Kalorite::MainWindow", "Open .lrc file")
                onClicked: if (lyrics) lyrics.openLrcFile()
                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(0, 0, 0, 0.25)
                }
            }

            ToolButton {
                implicitWidth: 26
                implicitHeight: 26
                icon.name: "view-refresh"
                ToolTip.visible: hovered
                ToolTip.text: qsTranslate("Kalorite::MainWindow", "Reload lyrics")
                onClicked: if (lyrics) lyrics.reload()
                background: Rectangle {
                    radius: 4
                    color: parent.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(0, 0, 0, 0.25)
                }
            }
        }

        MouseArea {
            id: controlsMouse
            anchors.top: parent.top
            anchors.right: parent.right
            width: 70
            height: 36
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }
    }
}
