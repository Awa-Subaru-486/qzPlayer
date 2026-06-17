import QtQuick
import QtQuick.Controls
import qz.theme
import qz.player

// 带快捷键提示气泡的图标按钮
BubbleButton {
    id: root
    property string text: ""
    property string key: ""

    hoverDelay: 500
    bubbleItem: bubble

    BubbleTip {
        id: bubble
        bubbleColor: Theme.isDark ? "#1a1a1a" : "#f0f0f0"
        width: row.implicitWidth + 20
        height: row.implicitHeight + 18

        Row {
            id: row
            anchors.centerIn: parent
            spacing: 5

            Text {
                visible: root.text !== ""
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                font.pixelSize: 13
                color: "#ffffff"
            }

            Rectangle {
                visible: root.key !== ""
                anchors.verticalCenter: parent.verticalCenter
                width: keyText.implicitWidth + 20
                height: keyText.implicitHeight + 6
                implicitWidth: keyText.implicitWidth + 20
                implicitHeight: keyText.implicitHeight + 6
                radius: 8
                color: "#ffffff"

                Text {
                    id: keyText
                    anchors.centerIn: parent
                    text: root.key
                    font.pixelSize: 13
                    font.bold: true
                    color: "#000000"
                }
            }
        }
    }
}
