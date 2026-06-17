import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import qz.theme
import qz.player
import qz.controls.md3

import "../controls"

// 播放器主控制栏，包含进度条、播放控制按钮、音量、倍速、设置等
Item { id: root
    property var window: Window.window
    property alias timeSlider: time_slider
    property var hotkeyManager: null

    property bool hasFullScreen: window ? window.visibility === Window.FullScreen : false

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
    property url mediaSource: ""
    property int aspectRatioMode: 0
    property int stretchMode: 0
    property var chapters: []
    property var subtitleStyle: ({})
    property double cacheValue: 0.0
    property bool audioVisualizationEnabled: false
    property bool settingsVisible: false

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
    signal subtitleToggleClicked()
    signal playlistToggleClicked()
    signal settingsClicked()
    signal videoStreamChanged(int index)
    signal audioStreamChanged(int index)
    signal subtitleStreamChanged(int index)
    signal subtitleStylePropChanged(string prop, var value)
    signal resetSubtitleStyle()
    signal audioVisualizationToggleClicked()
    signal chapterToggleClicked()

    width: controlShell.width
    height: controlShell.height

    Connections {
        target: root.hotkeyManager
        function onHotkeyChanged(action) {
            switch (action) {
            case HotkeyManager.PlayPause:   btn_play.key = root.hotkeyManager.hotkey(action); break
            case HotkeyManager.SeekForward:  btn_forward.key = root.hotkeyManager.hotkey(action); break
            case HotkeyManager.SeekBackward: btn_backward.key = root.hotkeyManager.hotkey(action); break
            case HotkeyManager.Mute:         btn_volume_icon.key = root.hotkeyManager.hotkey(action); break
            case HotkeyManager.Fullscreen:   btn_full_screen.key = root.hotkeyManager.hotkey(action); break
            }
        }
    }

    onIsPauseChanged: {
        if (isPause) {
            NotificationManager.show(qsTr("已暂停"), 0, "/res/icons/player/IconamoonPlayerPlayFill.svg")
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

    Rectangle { id: controlShell
        width: 780
        height: 78
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

        Item { id: top_item
            z: 1
            anchors {
                top: parent.top
                left: parent.left
                right: parent.right
                topMargin: 6
            }
            height: 14

            Text { id: current_text
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: 16
                }
                text: toTime(currentTime)
                font.pixelSize: 13
                color: Theme.textColor
            }

            PlayerSlider { id: time_slider
                anchors.centerIn: parent
                width: parent.width - ((duration_text.width + 24) * 2)
                height: 10
                to: duration
                mediaPlay: !root.isPause
                previewFrames: root.previewFrames
                mediaSource: root.mediaSource
                chapters: root.chapters
                cacheValue: root.cacheValue
                opening: PlayerSet.skipEnabled ? PlayerSet.openingDuration : 0
                ending: PlayerSet.skipEnabled ? PlayerSet.endingDuration : 0
            }

            Text { id: duration_text
                anchors{
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                    rightMargin: 16
                }
                text: toTime(duration)
                font.pixelSize: 13
                color: Theme.textColor
            }
        }

        Item { id: btn_item
            z: 2
            anchors {
                top: top_item.bottom
                bottom: parent.bottom
                left: parent.left
                right: parent.right
                leftMargin: 10
                rightMargin: 10
                topMargin: 8
                bottomMargin: 8
            }

            property int btnSize: 32
            property int iconSize: 26
            property int spacing: 5

            Row { id: left_row
                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                }
                spacing: 6

                KeyButton { id: btn_volume_icon
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: 20
                    source: volumeIcon()
                    text: root.muted ? qsTr("取消静音") : qsTr("静音")
                    key: root.hotkeyManager ? root.hotkeyManager.hotkey(HotkeyManager.Mute) : ""
                    onClicked: {
                        root.muted = !root.muted
                        root.muteChange(root.muted)
                    }

                    function volumeIcon() {
                        if (root.muted) return "/res/icons/player/FluentSpeakerMute48Filled.svg"
                        if (root.volume === 0) return "/res/icons/player/FluentSpeaker048Filled.svg"
                        if (root.volume <= 50) return "/res/icons/player/FluentSpeaker148Filled.svg"
                        return "/res/icons/player/FluentSpeaker48Filled.svg"
                    }
                }

                Md3Slider { id: volume_slider
                    z: isHovered ? 10 : 0
                    width: 110
                    height: btn_item.btnSize
                    from: 0
                    to: 100
                    value: root.volume
                    orientation: Qt.Horizontal

                    property bool isHovered: volume_mouseArea.containsMouse || volume_slider.pressed

                    MouseArea { id: volume_mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                        scrollGestureEnabled: true
                        acceptedButtons: Qt.NoButton

                        onWheel: function(wheel) {
                            volume_slider.value += (wheel.angleDelta.y > 0 ? 5 : -5)
                            volume_slider.value = Math.max(volume_slider.from, Math.min(volume_slider.to, volume_slider.value))
                            wheel.accepted = true
                        }
                    }

                    onValueChanged: {
                        if (Math.abs(value - root.volume) > 0.5) {
                            root.volume = value
                            root.volumeChange(value)
                            root.muted = false
                        }
                    }

                    Text { id: volume_value_text
                        anchors.left: parent.right
                        anchors.leftMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.volume.toFixed(0) + "%"
                        font.pixelSize: 12
                        color: Theme.textColor
                        opacity: volume_slider.isHovered ? 1.0 : 0.0

                        Behavior on opacity { NumberAnimation { duration: 150 } }
                    }
                }

                Item { id: volume_chapter_spacer
                    width: volume_slider.isHovered ? 36 : 0
                    height: 1
                    Behavior on width { NumberAnimation { duration: 150 } }
                }

                // 章节按钮
                IconButton {
                    visible: root.chapters.length > 0
                    source: "/res/icons/player/GgEreader.svg"
                    implicitWidth: 28
                    implicitHeight: 28
                    iconSize: 18
                    onClicked: root.chapterToggleClicked()
                }
            }

            RowLayout { id: center_row
                anchors.centerIn: parent
                spacing: btn_item.spacing

                KeyButton { id: btn_previous
                    Layout.alignment: Qt.AlignVCenter
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: "/res/icons/player/IconamoonPlayerPreviousFill.svg"
                    text: qsTr("上一个")
                    onClicked: root.previousClicked()
                }

                KeyButton { id: btn_backward
                    Layout.alignment: Qt.AlignVCenter
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: "/res/icons/player/IconamoonPlayerStartFill.svg"
                    text: qsTr("快退")
                    key: root.hotkeyManager ? root.hotkeyManager.hotkey(HotkeyManager.SeekBackward) : ""
                    onClicked: root.seekBackwardClicked()
                }

                KeyButton { id: btn_play
                    Layout.alignment: Qt.AlignVCenter
                    width: btn_item.btnSize + 10
                    height: btn_item.btnSize + 10
                    iconSize: btn_item.iconSize + 8
                    source: root.isPause ? "/res/icons/player/IconamoonPlayerPlayFill.svg" : "/res/icons/player/IconamoonPlayerPauseFill.svg"
                    text: root.isPause ? qsTr("播放") : qsTr("暂停")
                    key: root.hotkeyManager ? root.hotkeyManager.hotkey(HotkeyManager.PlayPause) : ""
                    onClicked: root.playPauseClicked()
                }

                KeyButton { id: btn_forward
                    Layout.alignment: Qt.AlignVCenter
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: "/res/icons/player/IconamoonPlayerEndFill.svg"
                    text: qsTr("快进")
                    key: root.hotkeyManager ? root.hotkeyManager.hotkey(HotkeyManager.SeekForward) : ""
                    onClicked: root.seekForwardClicked()
                }

                KeyButton { id: btn_next
                    Layout.alignment: Qt.AlignVCenter
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: "/res/icons/player/IconamoonPlayerNextFill.svg"
                    text: qsTr("下一个")
                    onClicked: root.nextClicked()
                }
            }

            Row { id: right_row
                anchors {
                    right: parent.right
                    verticalCenter: parent.verticalCenter
                }
                spacing: btn_item.spacing

                IconButton { id: btn_settings
                    source: "/res/icons/player/IconamoonSettingsFill.svg"
                    onClicked: {
                        root.settingsVisible = !root.settingsVisible
                        root.settingsClicked()
                    }
                }

                BubbleButton { id: btn_speed
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: speedIcon(root.currentSpeed)
                    hoverDelay: -1
                    bubbleItem: null

                    property bool bubbleShow: containsMouse || speed_bubble.containsMouse

                    onBubbleShowChanged: {
                        if (bubbleShow) {
                            speed_hide_timer.stop()
                            speed_bubble.show()
                        } else {
                            speed_hide_timer.start()
                        }
                    }

                    Timer { id: speed_hide_timer
                        interval: 100
                        onTriggered: {
                            if (!btn_speed.bubbleShow)
                                speed_bubble.hide()
                        }
                    }

                    function speedIcon(spd) {
                        if (spd === 2.0) return "/res/icons/player/MaterialSymbolsSpeed2xRounded.svg"
                        if (spd === 1.5) return "/res/icons/player/MaterialSymbolsSpeed15xRounded.svg"
                        if (spd === 0.75) return "/res/icons/player/MaterialSymbolsSpeed075Rounded.svg"
                        if (spd === 0.5) return "/res/icons/player/MaterialSymbolsSpeed05xRounded.svg"
                        return "/res/icons/player/MaterialSymbolsSpeedOutlineRounded.svg"
                    }

                    function setSpeed(spd) {
                        root.currentSpeed = spd
                        root.speedChange(spd)
                        speed_bubble.hide()
                    }

                    function speedFormat(spd) {
                        return spd.toFixed(1) + "x"
                    }

                    function applyCustomSpeed() {
                        var val = parseFloat(custom_speed_input.text)
                        if (!isNaN(val) && val >= 0.1 && val <= 4.0) {
                            setSpeed(val)
                            speed_custom_popup.close()
                        }
                    }

                    BubbleTip { id: speed_bubble
                        spacing: 0
                        width: 120
                        height: 184

                        Repeater {
                            model: [2.0, 1.5, 1.0, 0.75, 0.5]

                            Rectangle {
                                anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 8 + index * 30 }
                                width: speed_bubble.width - 16
                                height: 26
                                radius: 4
                                property bool isActive: root.currentSpeed === modelData
                                property bool isHovered: item_ma.containsMouse

                                color: isActive ? (Theme.isDark ? "#49454F" : "#80FFFFFF") :
                                       isHovered ? (Theme.isDark ? "#3a3a3a" : "#e8e8e8") : "transparent"
                                Behavior on color { ColorAnimation { duration: 100 } }

                                Text {
                                    anchors.centerIn: parent
                                    text: btn_speed.speedFormat(modelData)
                                    font.pixelSize: 13
                                    font.bold: isActive
                                    color: Theme.textColor
                                }

                                MouseArea {
                                    id: item_ma
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: btn_speed.setSpeed(modelData)
                                }
                            }
                        }

                        Rectangle {
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 158 }
                            width: speed_bubble.width - 16
                            height: 26
                            radius: 4
                            color: custom_ma.containsMouse ? (Theme.isDark ? "#3a3a3a" : "#e8e8e8") : "transparent"
                            Behavior on color { ColorAnimation { duration: 100 } }

                            Text {
                                anchors.centerIn: parent
                                text: qsTr("自定义...")
                                font.pixelSize: 13
                                color: Theme.textColor
                            }

                            MouseArea {
                                id: custom_ma
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    speed_bubble.hide()
                                    speed_custom_popup.open()
                                }
                            }
                        }
                    }

                    Md3Popup { id: speed_custom_popup
                        x: btn_speed.mapToItem(btn_speed.parent, 0, 0).x - width + btn_speed.width
                        y: btn_speed.mapToItem(btn_speed.parent, 0, 0).y - height - 8
                        width: 200
                        padding: 16

                        Column {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                text: qsTr("自定义倍速")
                                font.pixelSize: 14
                                font.bold: true
                                color: Theme.textColor
                            }

                            Md3TextField {
                                id: custom_speed_input
                                width: 168
                                font.pixelSize: 13
                                placeholderText: "0.1 ~ 4.0"
                                text: root.currentSpeed.toFixed(1)
                                validator: DoubleValidator { bottom: 0.1; top: 4.0; decimals: 1 }
                                clip: true

                                onActiveFocusChanged: {
                                    if (activeFocus) selectAll()
                                }

                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                        btn_speed.applyCustomSpeed()
                                    } else if (event.key === Qt.Key_Escape) {
                                        speed_custom_popup.close()
                                    }
                                }
                            }

                            Row {
                                anchors.horizontalCenter: parent.horizontalCenter
                                spacing: 8

                                Md3Button {
                                    text: qsTr("取消")
                                    onClicked: speed_custom_popup.close()
                                }

                                Md3Button {
                                    text: qsTr("确定")
                                    onClicked: btn_speed.applyCustomSpeed()
                                }
                            }
                        }

                        onOpened: {
                            custom_speed_input.text = root.currentSpeed.toFixed(1)
                            custom_speed_input.forceActiveFocus()
                        }
                    }
                }

                KeyButton { id: btn_playlist
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: "/res/icons/player/IconamoonPlaylist.svg"
                    text: qsTr("播放列表")
                    onClicked: {
                        root.playlistVisible = !root.playlistVisible
                        root.playlistToggleClicked()
                    }
                }

                KeyButton { id: btn_full_screen
                    width: btn_item.btnSize
                    height: btn_item.btnSize
                    iconSize: btn_item.iconSize
                    source: root.hasFullScreen ? "/res/icons/player/MaterialSymbolsFullscreenExitRounded.svg" : "/res/icons/player/MaterialSymbolsFullscreenRounded.svg"
                    text: root.hasFullScreen ? qsTr("退出全屏") : qsTr("全屏")
                    key: root.hotkeyManager ? root.hotkeyManager.hotkey(HotkeyManager.Fullscreen) : ""
                    onClicked: root.fullscreenClicked()
                }
            }
        }
    }

}
