import QtQuick

Window {
    id: root

    property string windowTitle: "About Kalorite"
    property string bodyHtml: ""
    property color background: "white"
    property color foreground: "black"
    property color linkColor: "blue"

    title: windowTitle
    width: 440
    height: 260
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    color: background
    modality: Qt.ApplicationModal

    onVisibleChanged: {
        if (visible) {
            physics.stop()
            logo.x = 16
            logo.y = 16
            logo.vx = 0
            logo.vy = 0
        }
    }

    Text {
        x: 140
        y: 24
        width: 280
        height: 212
        text: root.bodyHtml
        textFormat: Text.RichText
        wrapMode: Text.WordWrap
        verticalAlignment: Text.AlignTop
        color: root.foreground
        linkColor: root.linkColor
        onLinkActivated: function (link) {
            if (link.indexOf("http") === 0)
                Qt.openUrlExternally(link)
        }
    }

    Image {
        id: logo
        x: 16
        y: 16
        width: 96
        height: 96
        source: "image://app/logo"
        smooth: true
        property real vx: 0
        property real vy: 0
    }

    MouseArea {
        anchors.fill: logo
        cursorShape: Qt.PointingHandCursor
        onPressed: {
            if (!physics.running) {
                logo.vx = 3 + Math.floor(Math.random() * 6)
                logo.vy = -13 - Math.floor(Math.random() * 5)
                physics.start()
            } else {
                logo.vy -= 11 + Math.floor(Math.random() * 4)
                logo.vx += Math.floor(Math.random() * 11) - 5
            }
        }
    }

    Timer {
        id: physics
        interval: 16
        repeat: true
        onTriggered: {
            const gravity = 0.9
            const restitution = 0.78
            const friction = 0.98
            const maxX = root.width - logo.width
            const maxY = root.height - logo.height

            logo.vy += gravity
            logo.x += logo.vx
            logo.y += logo.vy

            if (logo.x < 0) {
                logo.x = 0
                logo.vx = -logo.vx * restitution
            }
            if (logo.x > maxX) {
                logo.x = maxX
                logo.vx = -logo.vx * restitution
            }
            if (logo.y < 0) {
                logo.y = 0
                logo.vy = -logo.vy * restitution
            }
            if (logo.y > maxY) {
                logo.y = maxY
                logo.vy = -logo.vy * restitution
                logo.vx *= friction
                if (Math.abs(logo.vy) < 1.6 && Math.abs(logo.vx) < 0.35) {
                    logo.vx = 0
                    logo.vy = 0
                    stop()
                }
            }
        }
    }
}
