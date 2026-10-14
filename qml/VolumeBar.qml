import QtQuick

Item {
    id: root

    implicitWidth: 80
    implicitHeight: 28

    // 0..100
    property int volume: 100

    // Параметры, повторяющие C++ реализацию
    readonly property int numBars: 10
    readonly property int spacing: 2
    readonly property color activeColor:   Qt.rgba(1, 1, 1, 240 / 255)
    readonly property color inactiveColor: Qt.rgba(1, 1, 1,  50 / 255)

    // Количество активных баров: ceil(volume / 100 * numBars)
    readonly property int activeBarsCount: volume > 0 ? Math.ceil(volume / 100 * numBars) : 0

    // Ширина каждого бара: barWidth = (w - totalSpacing) / numBars, min 2 (как целочисленное деление в C++)
    readonly property real barWidth: Math.max(2, Math.floor((width - spacing * (numBars - 1)) / numBars))

    Repeater {
        model: root.numBars

        Rectangle {
            id: bar

            readonly property real heightRatio: (index + 1) / root.numBars
            readonly property real barHeight: Math.max(2, Math.round(heightRatio * (root.height - 4)))
            readonly property bool isActive: (index < root.activeBarsCount && root.volume > 0)

            x: index * (root.barWidth + root.spacing)
            y: root.height - barHeight - 2
            width: root.barWidth
            height: barHeight

            radius: 1.5
            color: isActive ? root.activeColor : root.inactiveColor
            antialiasing: true
        }
    }

    // Обработка мыши — аналог mousePressEvent / mouseMoveEvent
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton

        function updateVolumeFromPos(mouseX) {
            if (root.width <= 0) return
            var ratio = Math.max(0.0, Math.min(1.0, mouseX / root.width))
            var vol = Math.round(ratio * 100)
            if (root.volume !== vol) {
                root.volume = vol
            }
        }

        onPressed: (mouse) => updateVolumeFromPos(mouse.x)
        onPositionChanged: (mouse) => {
            if (mouse.buttons & Qt.LeftButton)
                updateVolumeFromPos(mouse.x)
        }
    }
}
