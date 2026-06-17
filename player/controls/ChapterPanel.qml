import QtQuick
import qz.theme
import qz.player

// 章节面板内容组件
// 桌面端和移动端共享，通过 delegateHeight 和 showHoverEffect 参数化差异
Column {
    id: root

    required property var mediaPlayer
    property int delegateHeight: 36
    property bool showHoverEffect: true

    spacing: 4

    Text {
        text: qsTr("章节")
        font.pixelSize: 16
        font.bold: true
        color: Theme.textColor
        padding: 8
    }

    ListView {
        id: chapterListView
        width: parent.width
        height: parent.height - 36
        clip: true
        spacing: 2

        model: root.mediaPlayer ? root.mediaPlayer.chapters : []

        delegate: ChapterDelegate {
            delegateHeight: root.delegateHeight
            mediaPlayer: root.mediaPlayer
            showHoverEffect: root.showHoverEffect
            isCurrent: {
                var pos = root.mediaPlayer.position
                return modelData.startTime <= pos && modelData.endTime > pos
            }
        }
    }
}
