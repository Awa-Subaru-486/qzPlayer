import QtQuick
import qz.theme

// 播放列表项委托
Item {
    id: root

    required property int index
    required property string title
    required property url url
    required property var duration
    required property url coverUrl
    required property bool showHoverEffect
    required property bool useTapHandler
    required property bool isCurrent
    required property int minuteTick
    required property real position

    property int delegateHeight: 75

    width: parent ? parent.width : 0
    height: delegateHeight

    Rectangle {
        anchors.fill: parent
        anchors.rightMargin: 8
        anchors.leftMargin: 8
        radius: 6
        color: {
            if (isCurrent) return Theme.isDark ? "#49454F" : "#80FFFFFF"
            if (showHoverEffect && itemMouseArea.containsMouse) return Theme.isDark ? "#3a3a3a" : "#e8e8e8"
            return "transparent"
        }
    }

    Row {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        // 封面（如果有）16:9 比例，宽度 120
        RoundedImage {
            id: coverImage
            anchors.verticalCenter: parent.verticalCenter
            width: visible ? 120 : 0
            height: visible ? 120 * 9 / 16 : 0
            source: root.coverUrl
            visible: root.coverUrl.toString() !== ""
            fillMode: Image.PreserveAspectCrop
            radius: 4
        }

        // 垂直布局：标题 + 时间
        Column {
            id: infoColumn
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - parent.spacing - coverImage.width
            spacing: 12

            // 标题
            Text {
                width: parent.width
                text: root.title
                font.pixelSize: 16
                color: isCurrent ? (Theme.isDark ? "#d44e7d" : "#c12f7d") : Theme.textColor
                elide: Text.ElideRight
            }

            // 水平布局：时间显示
            Row {
                width: parent.width

                // 时间显示
                Text {
                    id: durationText
                    text: {
                        if (root.duration === undefined || root.duration === null) return "--:--"
                        var totalSeconds = Math.floor(root.duration / 1000)
                        var hours = Math.floor(totalSeconds / 3600)
                        var minutes = Math.floor((totalSeconds % 3600) / 60)
                        var seconds = totalSeconds % 60
                        if (hours > 0) {
                            return hours.toString().padStart(2, '0') + ":" +
                                   minutes.toString().padStart(2, '0') + ":" +
                                   seconds.toString().padStart(2, '0')
                        } else {
                            return minutes.toString().padStart(2, '0') + ":" +
                                   seconds.toString().padStart(2, '0')
                        }
                    }
                    font.pixelSize: 13
                    color: Theme.isDark ? "#888888" : "#666666"
                }

                Item { width: parent.width - endAtText.width - durationText.width; height: 1 }

                // 结束于时间
                Text {
                    id: endAtText
                    visible: root.isCurrent && root.duration > 0
                    text: {
                        var _tick = root.minuteTick
                        if (_tick < 0) return ""
                        var remainingMs = root.duration - root.position
                        if (remainingMs <= 0) return ""
                        var endTime = new Date(Date.now() + remainingMs)
                        var h = endTime.getHours().toString().padStart(2, '0')
                        var m = endTime.getMinutes().toString().padStart(2, '0')
                        return qsTr("结束于") + " " + h + ":" + m
                    }
                    font.pixelSize: 13
                    color: Theme.isDark ? "#888888" : "#666666"
                }
            }
        }
    }
}
