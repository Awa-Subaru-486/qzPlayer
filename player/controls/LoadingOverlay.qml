import QtQuick
import QtQuick.Controls
import qz.theme
import qz.controls.md3

// 加载状态覆盖层组件
Rectangle {
    id: root

    property bool isLoading: false

    width: 140
    height: 140
    radius: 20
    color: Theme.isDark ? "#cc1a1a1a" : "#ccf0f0f0"
    visible: isLoading
    opacity: isLoading ? 1.0 : 0.0

    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    Column {
        anchors.centerIn: parent
        spacing: 12

        Md3Loader {
            anchors.horizontalCenter: parent.horizontalCenter
            running: root.isLoading
            size: 56
            color: Theme.accentColor
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("加载中...")
            font.pixelSize: 14
            color: Theme.isDark ? "#cccccc" : "#666666"
        }
    }
}
