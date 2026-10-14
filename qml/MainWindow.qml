import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    implicitWidth: mainColumn.implicitWidth + 12
    implicitHeight: mainColumn.implicitHeight + topMenuBar.height + 10

    function tr(text) {
        return qsTranslate("Kalorite::MainWindow", text)
    }

    // Global keyboard shortcuts bound to MenuNode items
    Instantiator {
        model: mainWindow ? mainWindow.rootMenu.shortcuts : []
        delegate: Shortcut {
            sequence: modelData.shortcut
            onActivated: modelData.trigger()
        }
    }

    Shortcut {
        sequence: StandardKey.Find
        onActivated: {
            if (mainWindow) mainWindow.playlistVisible = true
            searchField.forceActiveFocus()
            searchField.selectAll()
        }
    }

    Component {
        id: menuComponent
        Menu {
            id: menu
            property var node: null
            title: node ? node.title : ""
            enabled: node ? node.enabled : true

            function build() {
                while (count > 0) removeItem(itemAt(0))
                if (!node) return
                for (let child of node.items) {
                    if (child.separator) {
                        addItem(sepComponent.createObject(null))
                    } else if (child.items.length > 0) {
                        let sub = menuComponent.createObject(null, { node: child })
                        sub.build()
                        addMenu(sub)
                    } else {
                        addItem(itemComponent.createObject(null, { node: child }))
                    }
                }
            }

            Component.onCompleted: build()

            Connections {
                target: menu.node
                function onItemsChanged() { menu.build() }
                function onChanged() {
                    menu.title = menu.node ? menu.node.title : ""
                    menu.enabled = menu.node ? menu.node.enabled : true
                }
            }
        }
    }

    Component {
        id: itemComponent
        MenuItem {
            id: item
            property var node: null
            text: node ? node.title : ""
            checkable: node ? node.checkable : false
            checked: node ? node.checked : false
            enabled: node ? node.enabled : true
            icon.name: node ? node.icon : ""
            onTriggered: if (node) node.trigger()

            Connections {
                target: item.node
                function onChanged() {
                    item.text = item.node ? item.node.title : ""
                    item.checkable = item.node ? item.node.checkable : false
                    item.checked = item.node ? item.node.checked : false
                    item.enabled = item.node ? item.node.enabled : true
                    item.icon.name = item.node ? item.node.icon : ""
                }
            }
        }
    }

    Component {
        id: sepComponent
        MenuSeparator {}
    }

    MenuBar {
        id: topMenuBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right

        Instantiator {
            model: mainWindow ? mainWindow.rootMenu.items : []
            delegate: Menu {
                id: topMenu
                property var node: modelData
                title: node ? node.title : ""
                enabled: node ? node.enabled : true

                function build() {
                    while (count > 0) removeItem(itemAt(0))
                    if (!node) return
                    for (let child of node.items) {
                        if (child.separator) {
                            addItem(sepComponent.createObject(null))
                        } else if (child.items.length > 0) {
                            let sub = menuComponent.createObject(null, { node: child })
                            sub.build()
                            addMenu(sub)
                        } else {
                            addItem(itemComponent.createObject(null, { node: child }))
                        }
                    }
                }

                Component.onCompleted: build()

                Connections {
                    target: topMenu.node
                    function onItemsChanged() { topMenu.build() }
                    function onChanged() {
                        topMenu.title = topMenu.node ? topMenu.node.title : ""
                        topMenu.enabled = topMenu.node ? topMenu.node.enabled : true
                    }
                }
            }
            onObjectAdded: (index, object) => topMenuBar.insertMenu(index, object)
            onObjectRemoved: (index, object) => topMenuBar.removeMenu(object)
        }
    }

    Menu {
        id: contextMenuPopup
        property var node: mainWindow ? mainWindow.contextMenu : null

        function build() {
            while (count > 0) removeItem(itemAt(0))
            if (!node) return
            for (let child of node.items) {
                if (child.separator) {
                    addItem(sepComponent.createObject(null))
                } else if (child.items.length > 0) {
                    let sub = menuComponent.createObject(null, { node: child })
                    sub.build()
                    addMenu(sub)
                } else {
                    addItem(itemComponent.createObject(null, { node: child }))
                }
            }
        }

        Component.onCompleted: build()

        Connections {
            target: contextMenuPopup.node
            function onItemsChanged() { contextMenuPopup.build() }
        }
    }

    function showContextMenu(x, y) {
        contextMenuPopup.build()
        contextMenuPopup.popup(x, y)
    }

    ColumnLayout {
        id: mainColumn
        anchors.top: topMenuBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 4
        anchors.bottomMargin: 6
        spacing: 6

        // Displays Row
        RowLayout {
            id: displaysRow
            Layout.fillWidth: true
            Layout.preferredHeight: 120
            Layout.maximumHeight: 120
            spacing: 6

            Item {
                id: winampContainer
                objectName: "winampContainer"
                Layout.fillWidth: true
                Layout.minimumWidth: 385
                Layout.preferredHeight: 120
                Layout.maximumHeight: 120
            }

            Item {
                id: patternContainer
                objectName: "patternContainer"
                visible: mainWindow ? mainWindow.patternVizVisible : true
                Layout.preferredWidth: 200
                Layout.preferredHeight: 120
                Layout.maximumWidth: 200
                Layout.maximumHeight: 120
            }
        }

        // Player Controls Row
        RowLayout {
            id: playerControlsRow
            Layout.fillWidth: true
            spacing: 6

            Button {
                implicitWidth: 32
                implicitHeight: 28
                icon.name: "media-skip-backward"
                ToolTip.visible: hovered
                ToolTip.text: root.tr("SkipBackward")
                enabled: mainWindow ? mainWindow.controlsEnabled : false
                onClicked: if (mainWindow) mainWindow.onSkipBack()
            }

            Button {
                implicitWidth: 32
                implicitHeight: 28
                icon.name: (mainWindow && mainWindow.isPlaying) ? "media-playback-pause" : "media-playback-start"
                ToolTip.visible: hovered
                ToolTip.text: root.tr("PlayTooltip")
                enabled: mainWindow ? mainWindow.controlsEnabled : false
                onClicked: if (mainWindow) mainWindow.onPlayClicked()
            }

            Button {
                implicitWidth: 32
                implicitHeight: 28
                icon.name: "media-skip-forward"
                ToolTip.visible: hovered
                ToolTip.text: root.tr("SkipForward")
                enabled: mainWindow ? mainWindow.controlsEnabled : false
                onClicked: if (mainWindow) mainWindow.onSkipNext()
            }

            Button {
                implicitWidth: 32
                implicitHeight: 28
                icon.name: mainWindow ? mainWindow.repeatIcon : "media-playlist-repeat"
                ToolTip.visible: hovered
                ToolTip.text: mainWindow ? mainWindow.repeatTooltip : ""
                enabled: mainWindow ? mainWindow.controlsEnabled : false
                onClicked: if (mainWindow) mainWindow.onRepeatClicked()
            }

            Slider {
                id: playbackSlider
                Layout.fillWidth: true
                from: 0
                to: 100
                value: mainWindow ? mainWindow.progress : 0
                enabled: mainWindow ? mainWindow.controlsEnabled : false

                onPressedChanged: {
                    if (pressed) {
                        if (mainWindow) mainWindow.onSliderPressed()
                    } else {
                        if (mainWindow) {
                            mainWindow.onSeek(Math.round(value))
                            mainWindow.onSliderReleased()
                        }
                    }
                }
            }

            VolumeBar {
                id: volumeBar
                Layout.preferredWidth: 80
                Layout.preferredHeight: 28
                volume: 100
                onVolumeChanged: if (mainWindow) mainWindow.onVolumeChanged(volume)
            }
        }

        // Equalizer Toggle Button
        Button {
            id: eqToggleBtn
            Layout.fillWidth: true
            implicitHeight: 28
            checkable: true
            checked: mainWindow ? mainWindow.eqVisible : false
            contentItem: Label {
                text: ((mainWindow && mainWindow.eqVisible) ? "▼ " : "▶ ") + root.tr("Equalizer")
                font.bold: true
                color: eqToggleBtn.palette.buttonText
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                leftPadding: 6
            }
            onClicked: if (mainWindow) mainWindow.eqVisible = !mainWindow.eqVisible
        }

        // Equalizer Panel
        EqualizerPanel {
            id: eqPanel
            visible: mainWindow ? mainWindow.eqVisible : false
            Layout.fillWidth: true
            implicitHeight: 160
        }

        // Playlist Toggle Button
        Button {
            id: playlistToggleBtn
            Layout.fillWidth: true
            implicitHeight: 28
            checkable: true
            checked: mainWindow ? mainWindow.playlistVisible : true
            contentItem: Label {
                text: ((mainWindow && mainWindow.playlistVisible) ? "▼ " : "▶ ") + root.tr("Playlist")
                font.bold: true
                color: playlistToggleBtn.palette.buttonText
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                leftPadding: 6
            }
            onClicked: if (mainWindow) mainWindow.playlistVisible = !mainWindow.playlistVisible
        }

        // Playlist Section
        ColumnLayout {
            id: playlistSection
            visible: mainWindow ? mainWindow.playlistVisible : true
            Layout.fillWidth: true
            spacing: 6

            TextField {
                id: searchField
                Layout.fillWidth: true
                implicitHeight: 28
                placeholderText: root.tr("Search...")
                selectByMouse: true
                leftPadding: 8
                rightPadding: searchField.text.length > 0 ? 26 : 8
                text: playlistModel ? playlistModel.filter : ""
                onTextChanged: {
                    if (playlistModel && playlistModel.filter !== text) {
                        playlistModel.filter = text
                    }
                }

                background: Rectangle {
                    radius: 6
                    color: searchField.palette.base
                    border.color: searchField.activeFocus ? searchField.palette.highlight : searchField.palette.mid
                    border.width: 1
                }

                ToolButton {
                    id: clearBtn
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.rightMargin: 4
                    visible: searchField.text.length > 0
                    implicitWidth: 20
                    implicitHeight: 20
                    padding: 0
                    ToolTip.text: root.tr("Clear search")
                    ToolTip.visible: hovered

                    contentItem: Label {
                        text: "✕"
                        font.pixelSize: 10
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: clearBtn.hovered ? searchField.palette.text : searchField.palette.placeholderText
                    }

                    background: Rectangle {
                        radius: 10
                        color: clearBtn.hovered ? (clearBtn.pressed ? searchField.palette.mid : searchField.palette.button) : "transparent"
                    }

                    onClicked: {
                        searchField.text = ""
                        searchField.forceActiveFocus()
                    }
                }

                Keys.onEscapePressed: {
                    text = ""
                }
            }

            PlaylistPanel {
                id: playlistPanel
                Layout.fillWidth: true
                implicitHeight: 160
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    icon.name: "list-add"
                    ToolTip.visible: hovered
                    ToolTip.text: root.tr("Add file to playlist")
                    onClicked: if (mainWindow) mainWindow.openButtonTriggered()
                }

                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    icon.name: "folder-new"
                    ToolTip.visible: hovered
                    ToolTip.text: root.tr("Add folder to playlist")
                    onClicked: if (mainWindow) mainWindow.addFolderTriggered()
                }

                Button {
                    implicitWidth: 32
                    implicitHeight: 32
                    icon.name: "media-optical"
                    ToolTip.visible: hovered
                    ToolTip.text: root.tr("Open Audio CD")
                    onClicked: if (mainWindow) mainWindow.openAudioCdTriggered()
                }

                Item { Layout.fillWidth: true }
            }
        }

        // Status Message Bar
        Label {
            text: mainWindow ? mainWindow.statusMessage : ""
            visible: text.length > 0
            font.pixelSize: 11
            Layout.fillWidth: true
            Layout.leftMargin: 4
            Layout.bottomMargin: 2
        }
    }
}
