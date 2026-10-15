import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Effects

Item {
    id: lyricsPanel

    implicitWidth: 0
    implicitHeight: 160

    property bool isSynced: true

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
        duration: 350
        easing.type: Easing.OutCubic
    }

    function smoothScrollTo(idx, force) {
        if ((!isSynced && !force) || idx < 0 || list.moving || list.flicking) return

        let item = list.itemAtIndex(idx)
        if (item) {
            // Exactly center the item vertically in the viewport
            let targetY = item.y - Math.round((list.height - item.height) / 2)
            let minY = -list.topMargin
            let maxY = Math.max(minY, list.contentHeight - list.height + list.bottomMargin)
            let clampedY = Math.max(minY, Math.min(targetY, maxY))

            if (Math.abs(list.contentY - clampedY) < 1) return

            scrollAnim.stop()
            scrollAnim.from = list.contentY
            scrollAnim.to = clampedY
            scrollAnim.start()
        } else {
            list.positionViewAtIndex(idx, ListView.Beginning)
            let targetY = list.contentY - Math.round((list.height - 40) / 2)
            let minY = -list.topMargin
            let maxY = Math.max(minY, list.contentHeight - list.height + list.bottomMargin)
            list.contentY = Math.max(minY, Math.min(targetY, maxY))
        }
    }

    function syncToCurrentLine() {
        lyricsPanel.isSynced = true
        if (lyrics) {
            let target = Math.max(0, lyrics.activeIndex)
            smoothScrollTo(target, true)
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
            cacheBuffer: 2500
            boundsBehavior: Flickable.StopAtBounds
            highlightRangeMode: ListView.NoHighlightRange
            keyNavigationEnabled: false

            onMovementStarted: {
                scrollAnim.stop()
                lyricsPanel.isSynced = false
            }

            onFlickStarted: {
                scrollAnim.stop()
                lyricsPanel.isSynced = false
            }

            Connections {
                target: typeof lyrics !== "undefined" ? lyrics : null
                function onActiveIndexChanged(idx) {
                    if (idx >= 0 && lyricsPanel.isSynced && !list.moving && !list.flicking) {
                        lyricsPanel.smoothScrollTo(idx, false)
                    }
                }
                function onHasLyricsChanged() {
                    lyricsPanel.isSynced = true
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

                property bool isCurrent: lyrics && lyrics.isSynced && lyrics.activeIndex >= 0 && (lineRow.index === lyrics.activeIndex)
                property bool isNext: lyrics && lyrics.isSynced && lyrics.activeIndex >= 0 && (lineRow.index === lyrics.activeIndex + 1)
                property bool hovered: rowMouse.containsMouse

                Text {
                    id: lineText
                    anchors.centerIn: parent
                    width: parent.width - 32
                    text: lineRow.text.length > 0 ? lineRow.text : "♪"
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.weight: Font.Bold
                    font.pixelSize: 18
                    renderType: Text.CurveRendering
                    color: "#ffffff"
                    opacity: lineRow.isCurrent ? 1.0 : (lineRow.hovered ? 0.80 : (lineRow.isNext ? 0.60 : 0.35))
                    scale: lineRow.isCurrent ? 1.0 : (lineRow.isNext ? 0.85 : 0.72)
                    transformOrigin: Item.Center

                    smooth: true
                    antialiasing: true

                    Behavior on opacity {
                        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                    }
                    Behavior on scale {
                        NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: !list.moving && !list.flicking
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        if (lyrics) {
                            lyrics.seekToLine(lineRow.index)
                            lyricsPanel.isSynced = true
                            lyricsPanel.smoothScrollTo(lineRow.index, true)
                        }
                    }
                }
            }
        }

        // Mask for progressive blur: transparent in center (sharp text), opaque at edges (max blur)
        Item {
            id: progressiveMask
            anchors.fill: list
            visible: false

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.00; color: "#ffffffff" }
                    GradientStop { position: 0.16; color: "#e6ffffff" }
                    GradientStop { position: 0.32; color: "#00ffffff" }
                    GradientStop { position: 0.68; color: "#00ffffff" }
                    GradientStop { position: 0.84; color: "#e6ffffff" }
                    GradientStop { position: 1.00; color: "#ffffffff" }
                }
            }
        }

        // Progressive edge blur & fade overlay (seamlessly intensifies toward borders, disappears on manual scroll)
        Item {
            id: blurContainer
            anchors.fill: list
            z: 5
            enabled: false

            property bool active: lyricsPanel.isSynced && lyrics && lyrics.hasLyrics && lyrics.isSynced

            opacity: active ? 1.0 : 0.0
            visible: opacity > 0.005

            Behavior on opacity {
                NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
            }

            ShaderEffectSource {
                id: listBlurSource
                anchors.fill: parent
                sourceItem: list
                live: blurContainer.visible
                hideSource: false
                visible: false
            }

            // Strong blur layer masked progressively toward edges
            MultiEffect {
                anchors.fill: parent
                source: listBlurSource
                blurEnabled: true
                blur: 1.0
                blurMax: 48
                blurMultiplier: 1.5
                maskEnabled: true
                maskSource: progressiveMask
            }

            // Atmospheric edge fade tint into background color
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.00; color: Qt.rgba(bgRect.color.r, bgRect.color.g, bgRect.color.b, 0.70) }
                    GradientStop { position: 0.18; color: Qt.rgba(bgRect.color.r, bgRect.color.g, bgRect.color.b, 0.30) }
                    GradientStop { position: 0.32; color: "transparent" }
                    GradientStop { position: 0.68; color: "transparent" }
                    GradientStop { position: 0.82; color: Qt.rgba(bgRect.color.r, bgRect.color.g, bgRect.color.b, 0.30) }
                    GradientStop { position: 1.00; color: Qt.rgba(bgRect.color.r, bgRect.color.g, bgRect.color.b, 0.70) }
                }
            }
        }

        // Top-right hover sensor for action buttons
        MouseArea {
            id: panelHoverArea
            anchors.top: parent.top
            anchors.right: parent.right
            width: 100
            height: 42
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            z: 1
        }

        // Floating action buttons (visible on panel hover in top right)
        Row {
            id: topActionsRow
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 6
            spacing: 4
            z: 20

            property bool showActions: panelHoverArea.containsMouse || openBtn.hovered || reloadBtn.hovered
            opacity: (showActions && lyrics && !lyrics.isLoading) ? 0.95 : 0.0
            visible: opacity > 0.01

            Behavior on opacity { NumberAnimation { duration: 180 } }

            ToolButton {
                id: openBtn
                implicitWidth: 26
                implicitHeight: 26
                padding: 0
                ToolTip.visible: hovered
                ToolTip.text: qsTranslate("Kalorite::MainWindow", "Open .lrc file")
                onClicked: if (lyrics) lyrics.openLrcFile()

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: 4
                    color: openBtn.pressed ? Qt.rgba(1, 1, 1, 0.35) : (openBtn.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(0, 0, 0, 0.35))
                    border.color: Qt.rgba(1, 1, 1, 0.15)
                    border.width: 1
                }

                contentItem: IconImage {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    name: "document-open"
                    color: "#ffffff"
                }
            }

            ToolButton {
                id: reloadBtn
                implicitWidth: 26
                implicitHeight: 26
                padding: 0
                ToolTip.visible: hovered
                ToolTip.text: qsTranslate("Kalorite::MainWindow", "Reload lyrics")
                onClicked: if (lyrics) lyrics.reload()

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: 4
                    color: reloadBtn.pressed ? Qt.rgba(1, 1, 1, 0.35) : (reloadBtn.hovered ? Qt.rgba(1, 1, 1, 0.25) : Qt.rgba(0, 0, 0, 0.35))
                    border.color: Qt.rgba(1, 1, 1, 0.15)
                    border.width: 1
                }

                contentItem: IconImage {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    name: "view-refresh"
                    color: "#ffffff"
                }
            }
        }

        // Floating sync button (bottom left, neat white circular button)
        ToolButton {
            id: syncBtn
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: 8
            implicitWidth: 30
            implicitHeight: 30
            z: 20
            padding: 0

            HoverHandler { cursorShape: Qt.PointingHandCursor }

            property bool shouldShow: !lyricsPanel.isSynced && lyrics && lyrics.hasLyrics && lyrics.isSynced

            visible: opacity > 0.01
            opacity: shouldShow ? 1.0 : 0.0
            scale: shouldShow ? 1.0 : 0.7

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
            Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

            ToolTip.visible: hovered
            ToolTip.text: qsTranslate("Kalorite::MainWindow", "Sync with playback")

            background: Item {
                anchors.fill: parent

                // Soft shadow
                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: 1
                    radius: width / 2
                    color: Qt.rgba(0, 0, 0, 0.25)
                }

                // White circle (no border)
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: syncBtn.pressed ? "#dedede" : (syncBtn.hovered ? "#f5f5f5" : "#ffffff")
                }
            }

            contentItem: IconImage {
                anchors.centerIn: parent
                width: Math.round(syncBtn.width / 2)
                height: Math.round(syncBtn.height / 2)
                sourceSize: Qt.size(width, height)
                name: "view-refresh"
                color: "#18181c"
            }

            onClicked: lyricsPanel.syncToCurrentLine()
        }
    }
}
