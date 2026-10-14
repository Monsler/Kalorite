import QtQuick
import QtQuick.Controls

Item {
    id: panel

    implicitHeight: 160

    property bool eqEnabled: false
    readonly property var bands: ["31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"]
    readonly property var presets: [
        { name: qsTr("Flat"), values: [50, 50, 50, 50, 50, 50, 50, 50, 50, 50] },
        { name: qsTr("Rock"), values: [65, 60, 55, 45, 42, 45, 52, 60, 65, 68] },
        { name: qsTr("Pop"), values: [40, 45, 55, 62, 65, 62, 55, 48, 42, 40] },
        { name: qsTr("Classical"), values: [60, 58, 55, 52, 48, 48, 52, 55, 58, 60] }
    ]

    property var sliders: []

    function setBand(index, value) {
        if (typeof eq !== "undefined") {
            eq.setBand(index, value)
        }
    }

    function applyPreset(presetValues) {
        for (var i = 0; i < presetValues.length; ++i) {
            if (sliders[i]) {
                sliders[i].value = presetValues[i]
                setBand(i, presetValues[i])
            }
        }
    }

    function setEnabled(on) {
        eqEnabled = on
        if (typeof eq !== "undefined") {
            eq.setEnabled(on)
        }
    }

    Row {
        id: header
        x: 10
        y: 8
        spacing: 8

        CheckBox {
            text: qsTr("Enabled")
            checked: panel.eqEnabled
            onToggled: panel.setEnabled(checked)
        }
        Button {
            width: 26
            height: 26
            icon.name: "edit-clear-all"
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Reset Equalizer")
            onClicked: panel.applyPreset(panel.presets[0].values)
        }
    }

    ScrollView {
        id: presetScroll
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.rightMargin: 10
        width: 70
        ScrollBar.vertical.policy: ScrollBar.AlwaysOff

        Column {
            width: presetScroll.availableWidth
            spacing: 4

            Label {
                text: qsTr("Presets:")
            }
            Repeater {
                model: panel.presets
                Button {
                    width: presetScroll.availableWidth
                    text: modelData.name
                    onClicked: panel.applyPreset(modelData.values)
                }
            }
        }
    }

    Row {
        x: 10
        y: 40
        width: presetScroll.x - 20

        Repeater {
            model: panel.bands

            Item {
                width: parent.width / panel.bands.length
                height: 116

                Slider {
                    id: bandSlider
                    anchors.horizontalCenter: parent.horizontalCenter
                    orientation: Qt.Vertical
                    from: 0
                    to: 100
                    value: 50
                    height: 92
                    enabled: panel.eqEnabled
                    onMoved: panel.setBand(index, Math.round(value))
                    Component.onCompleted: panel.sliders[index] = bandSlider
                }
                Label {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    font.pixelSize: 9
                }
            }
        }
    }
}
