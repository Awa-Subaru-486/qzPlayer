import QtQuick
import qz.theme
import qz.controls.md3
import qz.player

// MD3 风格通知组件，水平布局：图标 + 文本
// 固定在父元素水平中心、垂直75%处
Item { id: root

    property color notificationColor

    visible: false
    opacity: 0.0
    z: 100

    implicitWidth: Math.min(160, contentRow.implicitWidth + 34)
    implicitHeight: Math.min(80, contentRow.implicitHeight + 30)

    Connections {
        target: NotificationManager

        function onNotificationShown(text, duration, icon, color) {
            notificationText.text = text
            notificationIcon.source = icon || ""
            notificationIcon.visible = !!icon
            if (color && color !== Qt.rgba(0, 0, 0, 0)) {
                notificationColor = color
            } else {
                notificationColor = Theme.accentColor
            }

            hideAnim.stop()
            autoHideTimer.stop()
            visible = true
            showAnim.restart()

            if (duration > 0) {
                autoHideTimer.interval = duration * 1000
                autoHideTimer.start()
            }
        }

        function onNotificationClosed() {
            autoHideTimer.stop()
            hideNotification()
        }
    }

    function hideNotification() {
        hideAnim.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    y: parent.height * 0.25 - height / 2

    width: implicitWidth
    height: implicitHeight

    Rectangle { 
        anchors.fill: parent
        radius: Qt.platform.os === "android" ? 20 : 8
        color: Theme.isDark ? "#2a2a2a" : "#ffffff"
        opacity: Theme.isDark ? 0.85 : 0.9
    }

    Row { id: contentRow
        anchors.centerIn: parent
        spacing: 8

        SvgRenderer { id: notificationIcon
            width: 28
            height: 28
            paintedSize: 24
            color: Theme.isDark ? "#ffffff" : "#000000"
            visible: false
        }

        Text { id: notificationText
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 16
            font.bold: true
            color: Theme.isDark ? "#ffffff" : "#000000"
        }
    }

    Timer { id: autoHideTimer
        interval: 3000
        onTriggered: hideNotification()
    }

    NumberAnimation { id: showAnim
        target: root
        property: "opacity"
        to: 1.0
        duration: 200
        easing.type: Easing.OutCubic
    }

    SequentialAnimation { id: hideAnim
        NumberAnimation {
            target: root
            property: "opacity"
            to: 0.0
            duration: 200
            easing.type: Easing.InCubic
        }
        ScriptAction { script: root.visible = false }
    }
}
