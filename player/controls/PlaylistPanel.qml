import QtQuick
import QtQuick.Layouts
import qz.theme
import qz.player
import "../controls"

// 播放列表面板内容组件
// 桌面端和移动端共享，通过 showHoverEffect 参数化差异
Column {
    id: root

    required property var mediaPlayer
    required property var playlistModel
    property bool showHoverEffect: true
    property bool useTapHandler: false

    property int loopMode: 0
    signal loopModeChange(int mode)

    // 单一计时器，每分钟刷新一次，所有 delegate 共享
    property int _minuteTick: 0
    Timer {
        interval: 60000
        running: root.mediaPlayer && root.mediaPlayer.playing
        repeat: true
        onTriggered: root._minuteTick++
    }

    spacing: 4

    RowLayout {
        width: parent.width
        spacing: 4

        Text {
            Layout.fillWidth: true
            text: qsTr("播放列表")
            font.pixelSize: 16
            font.bold: true
            color: Theme.textColor
            padding: 8
        }

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 8
            source: loopModeIcon()
            onClicked: {
                var next = (root.loopMode + 1) % 3
                root.loopMode = next
                root.loopModeChange(next)
            }

            function loopModeIcon() {
                if (root.loopMode === 1) return "/res/icons/player/IconamoonPlaylistRepeatSong.svg"
                if (root.loopMode === 2) return "/res/icons/player/IconamoonPlaylistShuffle.svg"
                return "/res/icons/player/IconamoonPlaylistRepeatList.svg"
            }
        }
    }

    ListView {
        id: playlistView
        width: parent.width
        height: parent.height - 36
        spacing: 2

        model: root.playlistModel

        delegate: PlaylistDelegate {
            showHoverEffect: root.showHoverEffect
            useTapHandler: root.useTapHandler
            isCurrent: index === root.mediaPlayer.playlistIndex
            minuteTick: root._minuteTick
            position: root.mediaPlayer.position

            MouseArea {
                id: itemMouseArea
                anchors.fill: parent
                hoverEnabled: showHoverEffect
                cursorShape: Qt.PointingHandCursor
                enabled: !useTapHandler
                onClicked: {
                    mediaPlayer.playlistIndex = index
                    if (!mediaPlayer.playing) mediaPlayer.play()
                }
            }

            TapHandler {
                enabled: useTapHandler
                onTapped: {
                    mediaPlayer.playlistIndex = index
                    if (!mediaPlayer.playing) mediaPlayer.play()
                }
            }
        }
    }
}
