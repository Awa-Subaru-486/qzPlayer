import QtQuick
import qz.theme
import qz.player

// SVG图片按钮
Item {
    id: root
    property alias source: icon.source
    property color hoverColor: Theme.isDark ? "#555555" : "#F5F5F5"
    property alias containsMouse: mouseArea.containsMouse
    property bool active: false
    property color activeColor: Theme.accentColor
    property real iconAngle: 0
    property int iconSize: 24

    implicitWidth: 32
    implicitHeight: 32

    signal clicked()

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: mouseArea.containsMouse ? root.hoverColor : "transparent"
        Behavior on color { ColorAnimation { duration: 150 } }
    }

    SvgRenderer {
        id: icon
        anchors.centerIn: parent
        width: root.width - 5
        height: root.height - 5
        paintedSize: root.iconSize
        color: Theme.textColor
        rotation: root.iconAngle
        Behavior on rotation { NumberAnimation { duration: 200 } }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
