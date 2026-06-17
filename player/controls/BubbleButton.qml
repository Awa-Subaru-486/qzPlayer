import QtQuick
import qz.theme
import qz.player

// 带气泡提示的SVG图标按钮
Item {
    id: root
    implicitWidth: 32
    implicitHeight: 32

    property alias source: icon.source
    property alias color: icon.color
    property int iconSize: 24
    property color backgroundColor: "transparent"
    property color hoverColor: Theme.isDark ? "#555555" : "#F5F5F5"
    property alias containsMouse: mouseArea.containsMouse
    property int hoverDelay: 500
    property var bubbleItem: null

    signal clicked()

    onContainsMouseChanged: {
        if (!bubbleItem) return

        if (containsMouse) {
            if (hoverDelay <= 0) {
                bubbleItem.show()
            } else {
                delayTimer.start()
            }
        } else {
            delayTimer.stop()
            bubbleItem.hide()
        }
    }

    Timer {
        id: delayTimer
        interval: root.hoverDelay
        repeat: false
        onTriggered: {
            if (root.containsMouse && bubbleItem) {
                bubbleItem.show()
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }

    Item {
        id: buttonBody
        width: root.width
        height: root.height

        Rectangle {
            id: bg
            anchors.fill: parent
            radius: width / 2
            color: {
                if (!root.enabled) return Theme.isDark ? "#444" : "#e0e0e0"
                if (mouseArea.pressed) return Theme.isDark ? "#666" : "#d0d0d0"
                if (mouseArea.containsMouse) return root.hoverColor
                return root.backgroundColor
            }

            Behavior on color { ColorAnimation { duration: 150 } }
        }

        SvgRenderer {
            id: icon
            anchors.centerIn: parent
            width: root.iconSize
            height: root.iconSize
            paintedSize: root.iconSize
            color: {
                if (!root.enabled) return "#888888"
                return Theme.textColor
            }
        }
    }
}
