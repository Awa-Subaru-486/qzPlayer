import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import qz.theme
import qz.player
import qz.controls.md3
import "../controls"

// 移动端播放器控制栏 - 精简版，无音量/全屏，倍速和设置使用弹窗
Item { id: root
    property var window: Window.window
    property alias timeSlider: time_slider

    property double duration: 0
    property double currentTime: 0
    property bool isPause: true
    property int loopMode: 0
    property double currentSpeed: 1.0
    property int volume: 100
    property bool muted: false
    property bool playlistVisible: false
    property var videoStreams: []
    property var audioStreams: []
    property var subtitleStreams: []
    property int currentVideoStream: -1
    property int currentAudioStream: -1
    property int currentSubtitleStream: -1
    property var previewFrames: []
    property int aspectRatioMode: 0
    property var chapters: []
    property var subtitleStyle: ({})
    property double cacheValue: 0.0
    property bool audioVisualizationEnabled: false
    property bool settingsVisible: false
    readonly property bool isPortrait: Screen.primaryOrientation === Qt.PortraitOrientation

    signal playPauseClicked()
    signal speedChange(real speed)
    signal loopModeChange(int mode)
    signal volumeChange(int volume)
    signal muteChange(bool muted)
    signal fullscreenClicked()
    signal previousClicked()
    signal nextClicked()
    signal seekBackwardClicked()
    signal seekForwardClicked()
    signal chapterSeekRequested(double pos)
    signal playlistToggleClicked()
    signal settingsClicked()
    signal videoStreamChanged(int index)
    signal audioStreamChanged(int index)
    signal subtitleStreamChanged(int index)
    signal subtitleStylePropChanged(string prop, var value)
    signal resetSubtitleStyle()
    signal audioVisualizationToggleClicked()
    signal rotationToggleClicked()

    width: parent.width
    height: btn_item.height + controlShell.height

    onIsPauseChanged: {
        if (isPause) {
            NotificationManager.show(qsTr("已暂停"), 0, "/res/icons/player/IconamoonPlayerPauseFill.svg")
        } else {
            NotificationManager.close()
        }
    }

    onCurrentTimeChanged: {
        time_slider.setValue(currentTime)
    }

    function toTime(time) {
        var totalSeconds = Math.round(time);
        var hours = Math.floor(totalSeconds / 3600);
        var remainingSeconds = totalSeconds % 3600;
        var minutes = Math.floor(remainingSeconds / 60);
        var seconds = remainingSeconds % 60;

        var pad = function(num) { return num.toString().padStart(2, '0'); };

        if (hours > 0) {
            return pad(hours) + ":" + pad(minutes) + ":" + pad(seconds);
        } else {
            return pad(minutes) + ":" + pad(seconds);
        }
    }

    // 按钮区域
    Item { id: btn_item
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
        }
        height: 44

        RowLayout { id: button_row
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            // 上一个
            IconButton {
                Layout.alignment: Qt.AlignVCenter
                source: "/res/icons/player/IconamoonPlayerPreviousFill.svg"
                onClicked: root.previousClicked()
            }

            // 快退10s
            IconButton {
                visible: !root.isPortrait
                Layout.alignment: Qt.AlignVCenter
                source: "/res/icons/player/IconamoonPlayerStartFill.svg"
                onClicked: root.seekBackwardClicked()
            }

            // 播放/暂停
            IconButton {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 50
                implicitHeight: 50
                source: root.isPause
                    ? "/res/icons/player/IconamoonPlayerPlayFill.svg"
                    : "/res/icons/player/IconamoonPlayerPauseFill.svg"
                onClicked: root.playPauseClicked()
            }

            // 快进10s
            IconButton {
                visible: !root.isPortrait
                Layout.alignment: Qt.AlignVCenter
                source: "/res/icons/player/IconamoonPlayerEndFill.svg"
                onClicked: root.seekForwardClicked()
            }

            // 下一个
            IconButton {
                Layout.alignment: Qt.AlignVCenter
                source: "/res/icons/player/IconamoonPlayerNextFill.svg"
                onClicked: root.nextClicked()
            }
        }
    }

    // 进度条区域
    Rectangle { id: controlShell
        anchors {
            top: btn_item.bottom
            left: parent.left
            right: parent.right
        }
        height: 32
        radius: 12
        color: Theme.isDark ? "#dd1a1a1a" : "#ddf0f0f0"
        border.color: Theme.isDark ? "#33ffffff" : "#22000000"
        border.width: 1

        Rectangle {
            anchors.fill: parent
            radius: 12
            gradient: Gradient {
                GradientStop { position: 0.0; color: "transparent" }
                GradientStop { position: 1.0; color: Theme.isDark ? "#aa000000" : "#aaffffff" }
            }
        }

        Item { id: progress_item
            anchors {
                fill: parent
                leftMargin: 12
                rightMargin: 12
            }

            Text { id: current_text
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                }
                text: toTime(currentTime)
                font.pixelSize: 12
                color: Theme.textColor
            }

            PlayerSlider { id: time_slider
                anchors.centerIn: parent
                width: parent.width - ((duration_text.width + 16) * 2)
                height: 10
                to: duration
                mediaPlay: !root.isPause
                previewFrames: root.previewFrames
                chapters: root.chapters
                cacheValue: root.cacheValue
                opening: PlayerSet.skipEnabled ? PlayerSet.openingDuration : 0
                ending: PlayerSet.skipEnabled ? PlayerSet.endingDuration : 0
            }

            Text { id: duration_text
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                text: toTime(duration)
                font.pixelSize: 12
                color: Theme.textColor
            }
        }
    }
}
