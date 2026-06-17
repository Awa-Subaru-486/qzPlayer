import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import qz.theme
import qz.player
import qz.multimedia
import qz.controls.md3
import "../controls"

Window {
    id: window
    width: Screen.width
    height: Screen.height
    visible: false
    visibility: Window.FullScreen
    color: "#000000"

    property bool isShowControls: true
    property var previewFrames: []
    property bool isLocked: false
    property bool _lockButtonVisible: false
    property int _brightness: androidUtils.systemBrightness()
    property int _maxBrightness: 255
    property bool _wasPlayingBeforeSuspend: false
    property real _speedBeforeLongPress: 1.0
    property bool _longPressSpeedActive: false
    property bool _gestureAdjusting: false
    property bool _swipeSeeking: false
    property real _swipeSeekPosition: 0

    readonly property bool isLandscape: window.width > window.height
    readonly property int playlistCount: player.playlist.length

    AndroidUtils { id: androidUtils }

    // 共享状态管理
    PlayerState {
        id: playerState
        mediaPlayer: player
    }

    // 播放列表数据模型
    PlaylistModel {
        id: playlistModel
    }

    InputMonitor {
        id: mouseMonitor

        onSingleTapped: {
            if (window._gestureAdjusting) return
            if (window.isLocked) {
                window._lockButtonVisible = !window._lockButtonVisible
                if (window._lockButtonVisible) {
                    lockHideTimer.restart()
                }
                return
            }
            if (playerState.playlistPanelVisible) {
                playerState.playlistPanelVisible = false
                return
            }
            if (playerState.settingsPanelVisible) {
                playerState.settingsPanelVisible = false
                return
            }
            if (window.isShowControls) {
                hideControls()
            } else {
                showControls()
            }
        }

        onDoubleTapped: {
            if (window._gestureAdjusting) return
            if (window.isLocked) return
            if (player.playing) {
                player.pause()
            } else {
                if (player.mediaStatus === MediaPlayer.EndOfMedia) {
                    player.position = 0
                }
                player.play()
            }
        }

        onLongPressed: {
            if (window._gestureAdjusting) return
            if (window.isLocked) return
            window._speedBeforeLongPress = player.playbackRate
            player.playbackRate = 2.0
            window._longPressSpeedActive = true
            NotificationManager.show("2x", 0, "/res/icons/player/IconamoonPlayerPlayFill.svg")
        }

        onLongPressReleased: {
            if (window._gestureAdjusting) return
            player.playbackRate = window._speedBeforeLongPress
            window._longPressSpeedActive = false
            NotificationManager.close()
        }

        onHorizontalSwipe: {
            if (window.isLocked) return
            if (window._gestureAdjusting) return
            if (!window._swipeSeeking) {
                window._swipeSeeking = true
                window._swipeSeekPosition = player.position
                hideControls()
            }
            var deltaMs = (dx / videoContainer.width) * player.duration * 0.3
            window._swipeSeekPosition = Math.max(0, Math.min(player.duration, window._swipeSeekPosition + deltaMs))
            seekIndicator.visible = true
        }

        onHorizontalSwipeReleased: {
            if (!window._swipeSeeking) return
            window._swipeSeeking = false
            player.position = Math.round(window._swipeSeekPosition)
            seekIndicator.visible = false
        }
    }

    function formatTime(ms) {
        var time = Math.round(ms / 1000);
        var hours = Math.floor(time / 3600);
        var minutes = Math.floor((time % 3600) / 60);
        var seconds = Math.floor(time % 60);
        var pad = function(num) { return num.toString().padStart(2, '0'); };
        if (hours > 0) {
            return pad(hours) + ":" + pad(minutes) + ":" + pad(seconds);
        } else {
            return pad(minutes) + ":" + pad(seconds);
        }
    }

    Component.onCompleted: {
        VulkanWindowInitializer.initialize(window)
        window.visible = true
        updateMonitorRegion()
        showControls()
    }

    function updateMonitorRegion() {
         mouseMonitor.setRegion(
             lockButton.x + lockButton.width,
             titleBar.height,
             videoContainer.width - lockButton.width - rightQuickButtons.width - 20,
             videoContainer.height - playerControl.height
         )
    }

    onWidthChanged: updateMonitorRegion()
    onHeightChanged: updateMonitorRegion()

    // Android 返回键：直接移到后台
    Connections {
        target: androidUtils
        function onBackPressed() {
            androidUtils.moveTaskToBack()
        }
    }

    // Android 生命周期管理
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state === Qt.ApplicationSuspended) {
                _wasPlayingBeforeSuspend = player.playing
                if (player.playing) {
                    player.pause()
                }
            } else if (Qt.application.state === Qt.ApplicationActive) {
                androidUtils.applyWindowBrightness(_brightness)
                if (_wasPlayingBeforeSuspend) {
                    player.play()
                    _wasPlayingBeforeSuspend = false
                }
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 5000
        onTriggered: {
            if (playerState.anyPanelVisible) return
            hideControls()
        }
    }

    Timer {
        id: lockHideTimer
        interval: 3000
        onTriggered: {
            window._lockButtonVisible = false
        }
    }

    function showControls() {
        if (!window.isShowControls) {
            window.isShowControls = true
        }
        hideTimer.restart()
    }

    function hideControls() {
        if (window.isShowControls) {
            window.isShowControls = false
        }
    }

    function playAtIndex(index) {
        player.playlistIndex = index
    }

    function setPlaylist(urls) {
        playlistModel.clear()
        playlistModel.addUrls(urls)
        player.setPlaylist(urls)
    }

    function addToPlaylist(urls) {
        if (player.playlist.length === 0) {
            setPlaylist(urls)
        } else {
            var startIdx = player.playlist.length
            playlistModel.addUrls(urls)
            player.addToPlaylist(urls)
            player.playlistIndex = startIdx
        }
    }

    function play() {
        player.play()
    }

    MediaPlayer { id: player
        videoOutput: videoOutput
        audioOutput: audioOutput
        audioBufferOutput: audioBufferOutput
        playbackMode: MediaPlayer.LoopPlayback
        opening: PlayerSet.skipEnabled ? PlayerSet.openingDuration : 0
        ending: PlayerSet.skipEnabled ? PlayerSet.endingDuration : 0
        hdrEnabled: PlayerSet.hdrEnabled
        zeroCopyEnabled: PlayerSet.zeroCopyEnabled
        lowLatencyStreamingEnabled: PlayerSet.lowLatencyStreamingEnabled

        onPlaylistIndexChanged: function(index) {
            if (index >= 0 && index < player.playlist.length) {
                var title = player.metaData.stringValue(0)
                if (title && title.length > 0) {
                    window.title = title
                } else {
                    var url = player.playlist[index]
                    window.title = url.toString().split("/").pop()
                }
            }
        }

        onMetaDataChanged: {
            var title = player.metaData.stringValue(0)
            if (title && title.length > 0) {
                window.title = title
            }
        }
    }

    AudioOutput {
        id: audioOutput
        volume: 1.0
        muted: false
    }

    AudioBufferOutput {
        id: audioBufferOutput
    }

    Item {
        id: videoContainer
        anchors.top: parent.top
        x: 0
        width: {
            if (window.isLandscape) {
                if (playerState.chapterPanelVisible)
                    return window.width - chapterPanel.width - 10
                if (playerState.playlistPanelVisible)
                    return window.width - playlistPanel.width - 10
                if (playerState.settingsPanelVisible)
                    return window.width - settingsPanel.width - 10
            }
            return window.width
        }
        height: {
            if (!window.isLandscape) {
                if (playerState.chapterPanelVisible)
                    return window.height - chapterPanel.height - 10
                if (playerState.playlistPanelVisible)
                    return window.height - playlistPanel.height - 10
                if (playerState.settingsPanelVisible)
                    return window.height - settingsPanel.height - 10
            }
            return window.height
        }

        Behavior on width { NumberAnimation { duration: 300; easing.type: Easing.InOutCubic } }
        Behavior on height { NumberAnimation { duration: 300; easing.type: Easing.InOutCubic } }

        onWidthChanged: updateMonitorRegion()
        onHeightChanged: updateMonitorRegion()

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            radius: playerState.anyPanelVisible ? 8 : 0

            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                radius: playerState.anyPanelVisible ? 8 : 0
                visible: !playerState.audioVisualizationEnabled
            }

            AudioVisualizer {
                id: audioVisualizer
                anchors.fill: parent
                audioBufferOutput: audioBufferOutput
                style: AudioVisualizer.BarSpectrum
                barCount: 64
                color: Theme.isDark ? "#d44e7d" : "#c12f7d"
                visible: playerState.audioVisualizationEnabled
            }

            LoadingOverlay {
                id: loadingOverlay
                anchors.centerIn: parent
                z: 100
                isLoading: player.buffering
            }

            NotificationBar {
                z: 101
            }

            // 滑动 Seek 指示器
            Rectangle {
                id: seekIndicator
                visible: false
                z: 101
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.verticalCenter
                anchors.bottomMargin: parent.height * 0.25
                width: 100
                height: 56
                radius: 14
                color: Theme.isDark ? "#b8222222" : "#b8e0e0e0"

                Column {
                    anchors.centerIn: parent
                    spacing: 2

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: {
                            var delta = Math.round((window._swipeSeekPosition - player.position) / 1000)
                            var abs = Math.abs(delta)
                            var h = Math.floor(abs / 3600)
                            var m = Math.floor((abs % 3600) / 60)
                            var s = abs % 60
                            var str = ""
                            if (h > 0) str += h + "h"
                            if (m > 0) str += m + "m"
                            if (s > 0 || str === "") str += s + "s"
                            return (delta >= 0 ? "+" : "-") + str
                        }
                        font.pixelSize: 18
                        font.bold: true
                        color: Theme.accentColor
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: formatTime(window._swipeSeekPosition)
                        font.pixelSize: 11
                        color: Theme.isDark ? "#aaffffff" : "#aa000000"
                    }
                }
            }

            // 滑动 Seek 进度条
            Md3Slider {
                id: seekSlider
                z: 101
                visible: seekIndicator.visible
                width: videoContainer.width * 0.75
                height: 36
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: seekIndicator.top
                anchors.bottomMargin: 8
                hideHandle: true
                from: 0
                to: player.duration
                value: window._swipeSeekPosition
                enabled: false
            }

            // 左侧手势区域 - 调节亮度
            Item {
                id: leftGestureArea
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height * 0.8
                width: parent.width * 0.45
                z: 8
                enabled: !window.isLocked && !window._swipeSeeking

                property real _startBrightness: 0
                property bool _dragging: false

                DragHandler {
                    id: leftDrag
                    target: null
                    yAxis.enabled: true
                    xAxis.enabled: false
                    enabled: !window.isLocked && !window._swipeSeeking

                    onActiveChanged: {
                        if (active) {
                            leftGestureArea._startBrightness = window._brightness
                            leftGestureArea._dragging = true
                            window._gestureAdjusting = true
                            brightnessIndicator.visible = true
                            hideControls()
                        } else {
                            leftGestureArea._dragging = false
                            window._gestureAdjusting = false
                            brightnessIndicator.visible = false
                        }
                    }

                    onActiveTranslationChanged: {
                        if (!leftGestureArea._dragging) return
                        var brightnessDelta = (-activeTranslation.y / leftGestureArea.height) * window._maxBrightness
                        var newBrightness = Math.max(0, Math.min(window._maxBrightness,
                            leftGestureArea._startBrightness + brightnessDelta))
                        window._brightness = Math.round(newBrightness)
                        androidUtils.setSystemBrightness(window._brightness)
                        brightnessIndicator.value = window._brightness / window._maxBrightness
                    }
                }
            }

            // 右侧手势区域 - 调节音量
            Item {
                id: rightGestureArea
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: parent.height * 0.8
                width: parent.width * 0.45
                z: 8
                enabled: !window.isLocked && !window._swipeSeeking

                property real _startVolume: 0
                property bool _dragging: false

                DragHandler {
                    id: rightDrag
                    target: null
                    yAxis.enabled: true
                    xAxis.enabled: false
                    enabled: !window.isLocked && !window._swipeSeeking

                    onActiveChanged: {
                        if (active) {
                            rightGestureArea._startVolume = androidUtils.systemVolume(AndroidUtils.StreamMusic)
                            rightGestureArea._dragging = true
                            window._gestureAdjusting = true
                            volumeIndicator.visible = true
                            hideControls()
                        } else {
                            rightGestureArea._dragging = false
                            window._gestureAdjusting = false
                            volumeIndicator.visible = false
                        }
                    }

                    onActiveTranslationChanged: {
                        if (!rightGestureArea._dragging) return
                        var maxVol = androidUtils.maxSystemVolume(AndroidUtils.StreamMusic)
                        var volumeDelta = (-activeTranslation.y / rightGestureArea.height) * maxVol
                        var newVolume = Math.max(0, Math.min(maxVol,
                            rightGestureArea._startVolume + volumeDelta))
                        androidUtils.setSystemVolume(AndroidUtils.StreamMusic, Math.round(newVolume))
                        volumeIndicator.value = Math.round(newVolume) / maxVol
                    }
                }
            }

            // 亮度指示器
            Rectangle {
                id: brightnessIndicator
                visible: false
                z: 20
                width: 60
                height: 160
                radius: 16
                color: Theme.isDark ? "#dd1a1a1a" : "#ddf0f0f0"
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter

                property real value: window._brightness / window._maxBrightness

                Row {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Item {
                        width: 24
                        height: 24
                        anchors.verticalCenter: parent.verticalCenter

                        SvgRenderer {
                            anchors.fill: parent
                            paintedSize: 24
                            source: brightnessIndicator.value <= 0.25
                                ? "/res/icons/player/MaterialSymbolsBrightness4OutlineRounded.svg"
                                : "/res/icons/player/MaterialSymbolsBrightness5Rounded.svg"
                            color: Theme.textColor
                        }
                    }

                    Rectangle {
                        width: 16
                        height: parent.height
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 8
                        color: Theme.isDark ? "#33ffffff" : "#22000000"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            height: parent.height * brightnessIndicator.value
                            radius: 8
                            color: Theme.accentColor
                            Behavior on height { NumberAnimation { duration: 50 } }
                        }
                    }
                }
            }

            // 音量指示器
            Rectangle {
                id: volumeIndicator
                visible: false
                z: 20
                width: 60
                height: 160
                radius: 16
                color: Theme.isDark ? "#dd1a1a1a" : "#ddf0f0f0"
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.verticalCenter: parent.verticalCenter

                property real value: androidUtils.systemVolume(AndroidUtils.StreamMusic) /
                                     Math.max(1, androidUtils.maxSystemVolume(AndroidUtils.StreamMusic))

                Row {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 4

                    Rectangle {
                        width: 16
                        height: parent.height
                        anchors.verticalCenter: parent.verticalCenter
                        radius: 8
                        color: Theme.isDark ? "#33ffffff" : "#22000000"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: parent.width
                            height: parent.height * volumeIndicator.value
                            radius: 8
                            color: Theme.accentColor
                            Behavior on height { NumberAnimation { duration: 50 } }
                        }
                    }

                    Item {
                        width: 24
                        height: 24
                        anchors.verticalCenter: parent.verticalCenter

                        SvgRenderer {
                            anchors.fill: parent
                            paintedSize: 24
                            source: {
                                if (volumeIndicator.value === 0) {
                                    return "/res/icons/player/FluentSpeakerMute48Filled.svg"
                                } else if (volumeIndicator.value <= 0.33) {
                                    return "/res/icons/player/FluentSpeaker048Filled.svg"
                                } else if (volumeIndicator.value <= 0.66) {
                                    return "/res/icons/player/FluentSpeaker148Filled.svg"
                                } else {
                                    return "/res/icons/player/FluentSpeaker48Filled.svg"
                                }
                            }
                            color: Theme.textColor
                        }
                    }
                }
            }
        }
    }

    PlayerTitleBar { id: titleBar
        anchors.left: videoContainer.left
        width: videoContainer.width
        z: 10
        y: (playerState.anyPanelVisible || window.isLocked) ? -height :
           window.isShowControls ? 0 : -height
        Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        titleText: window.title
        mediaPlayer: player
        chapters: player.chapters

        onCloseClicked: androidUtils.moveTaskToBack()
        onMiniModeClicked: {
            if (player.playing) {
                hideControls();
                playerState.hideAllPanels()
                androidUtils.enterPictureInPicture(16, 9)
            }
        }
        onRotationToggleClicked: androidUtils.toggleOrientation()
        onSettingsClicked: playerState.toggleSettings()
        onChapterClicked: playerState.toggleChapter()
    }

    // 锁定按钮
    IconButton {
        id: lockButton
        x: (playerState.anyPanelVisible || (!window.isLocked && !window.isShowControls)) ? -width - 10 : 10
        anchors.verticalCenter: parent.verticalCenter
        z: 10
        opacity: {
            if (playerState.anyPanelVisible) return 0
            if (window.isLocked) return window._lockButtonVisible ? 1 : 0
            return 1
        }
        Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 200 } }
        source: window.isLocked
            ? "/res/icons/player/MaterialSymbolsLock.svg"
            : "/res/icons/player/MaterialSymbolsLockOpenRounded.svg"
        onClicked: {
            window.isLocked = !window.isLocked
            if (window.isLocked) {
                hideControls()
                window._lockButtonVisible = true
                lockHideTimer.restart()
            } else {
                window._lockButtonVisible = false
                showControls()
            }
        }
    }

    // 右侧快捷按钮组
    Column {
        id: rightQuickButtons
        x: (playerState.anyPanelVisible || window.isLocked || !window.isShowControls) ? parent.width + 10 : parent.width - width - 10
        anchors.verticalCenter: parent.verticalCenter
        z: 10
        spacing: 8
        opacity: (playerState.anyPanelVisible || window.isLocked) ? 0 : 1
        Behavior on x { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 200 } }

        IconButton {
            source: "/res/icons/player/IconamoonPlaylist.svg"
            onClicked: playerState.togglePlaylist()
        }

        IconButton {
            id: speedButton
            active: player.playbackRate !== 1.0
            source: speedIcon()
            onClicked: speedPopup.visible ? speedPopup.close() : speedPopup.open()

            function speedIcon() {
                if (player.playbackRate === 2.0) return "/res/icons/player/MaterialSymbolsSpeed2xRounded.svg"
                if (player.playbackRate === 1.5) return "/res/icons/player/MaterialSymbolsSpeed15xRounded.svg"
                return "/res/icons/player/MaterialSymbolsSpeedOutlineRounded.svg"
            }
        }
    }

    // 速度弹窗
    Popup {
        id: speedPopup
        parent: window.contentItem
        x: rightQuickButtons.x - width - 10
        y: rightQuickButtons.y + rightQuickButtons.height / 2 - height / 2
        padding: 6
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            radius: Theme.roundedScale
            color: Theme.isDark ? "#cc2a2a2a" : "#cce0e0e0"
            border.color: Theme.isDark ? "#33ffffff" : "#22000000"
            border.width: 1
        }

        contentItem: Column {
            spacing: 2

            Repeater {
                model: [0.5, 1.0, 1.5, 2.0]

                Rectangle {
                    width: 50
                    height: 42
                    radius: width / 2
                    color: player.playbackRate === modelData
                        ? (Theme.isDark ? "#55ffffff" : "#55000000")
                        : (sp_ma.containsMouse ? (Theme.isDark ? "#33ffffff" : "#33000000") : "transparent")
                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: modelData + "x"
                        font.pixelSize: 13
                        color: Theme.textColor
                    }

                    MouseArea {
                        id: sp_ma
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            player.playbackRate = modelData
                            speedPopup.close()
                        }
                    }
                }
            }
        }
    }

    // 章节面板 - 使用共享 SlidePanel + ChapterPanel
    SlidePanel {
        id: chapterPanel
        z: 5
        panelWidth: window.isLandscape ? Math.min(260, window.width * 0.7) : Math.min(260, window.height * 0.7)
        edge: window.isLandscape ? SlidePanel.Edge.Right : SlidePanel.Edge.Bottom
        isOpen: playerState.chapterPanelVisible

        ChapterPanel {
            anchors.fill: parent
            mediaPlayer: player
            delegateHeight: 48
            showHoverEffect: false
        }
    }

    // 播放列表面板 - 使用共享 SlidePanel + PlaylistPanel
    SlidePanel {
        id: playlistPanel
        z: 5
        panelWidth: window.isLandscape ? window.width * 0.4 : window.height * 0.4
        edge: window.isLandscape ? SlidePanel.Edge.Right : SlidePanel.Edge.Bottom
        isOpen: playerState.playlistPanelVisible

        PlaylistPanel {
            anchors.fill: parent
            mediaPlayer: player
            playlistModel: playlistModel
            showHoverEffect: false
            useTapHandler: true
            loopMode: player.playbackMode === MediaPlayer.CurrentItemPlayback ? 1 :
                      player.playbackMode === MediaPlayer.RandomPlayback ? 2 : 0

            onLoopModeChange: function(mode) {
                if (mode === 1) player.playbackMode = MediaPlayer.CurrentItemPlayback
                else if (mode === 2) player.playbackMode = MediaPlayer.RandomPlayback
                else player.playbackMode = MediaPlayer.LoopPlayback
            }
        }
    }

    // 设置面板 - 使用共享 SlidePanel + SettingsBubble
    SlidePanel {
        id: settingsPanel
        z: 5
        panelWidth: window.isLandscape ? window.width * 0.45 : window.height * 0.45
        edge: window.isLandscape ? SlidePanel.Edge.Right : SlidePanel.Edge.Bottom
        isOpen: playerState.settingsPanelVisible

        SettingsBubble { id: settings_bubble
            anchors.fill: parent

            videoStreams: player.videoTracks
            audioStreams: player.audioTracks
            subtitleStreams: player.subtitleTracks
            currentVideoStream: player.activeVideoTrack
            currentAudioStream: player.activeAudioTrack
            currentSubtitleStream: player.activeSubtitleTrack
            isHdrEnabled: PlayerSet.hdrEnabled
            isZeroCopyEnabled: PlayerSet.zeroCopyEnabled
            isLowLatencyStreamingEnabled: PlayerSet.lowLatencyStreamingEnabled
            aspectRatioMode: playerState.aspectRatioMode
            subtitleStyle: player.subtitleStyle
            audioVisualizationEnabled: playerState.audioVisualizationEnabled
            showStretchMode: false
            decoderPriorityList: player.videoDecoderPriority()
            activeVideoDecoder: player.activeDecoder

            onHdrToggleClicked: {
                // HDR 通过 VideoSink 即时生效，无需重启
            }
            onZeroCopyToggleClicked: {
            }
            onLowLatencyStreamingToggleClicked: {
                if (player.playing) {
                    NotificationManager.show(qsTr("需要重新启动程序后生效"), 3)
                }
            }
            onVideoStreamChanged: function(index) { player.setActiveVideoTrack(index) }
            onAudioStreamChanged: function(index) { player.setActiveAudioTrack(index) }
            onSubtitleStreamChanged: function(index) { player.setActiveSubtitleTrack(index) }
            onAspectRatioSelected: function(mode) { playerState.aspectRatioMode = mode }
            onSubtitleStylePropChanged: function(prop, value) { playerState.applySubtitleStyleProp(prop, value) }
            onResetSubtitleStyle: playerState.resetSubtitleStyle()
            onAudioVisualizationToggleClicked: playerState.audioVisualizationEnabled = !playerState.audioVisualizationEnabled
            onDecoderPrioritized: function(decoderId) { player.prioritizeDecoder(decoderId) }
            onDecoderDeprioritized: function(decoderId) { player.deprioritizeDecoder(decoderId) }
        }
    }

    PlayerControl {
        id: playerControl
        width: videoContainer.width
        anchors.left: videoContainer.left
        z: 10
        visible: true
        y: (playerState.anyPanelVisible || window.isLocked || !window.isShowControls) ? parent.height : parent.height - height - 5
        Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        duration: player.duration / 1000
        currentTime: player.position / 1000
        cacheValue: player.bufferProgress * player.duration / 1000
        isPause: !player.playing
        previewFrames: window.previewFrames
        aspectRatioMode: playerState.aspectRatioMode
        settingsVisible: playerState.settingsPanelVisible

        videoStreams: player.videoTracks
        audioStreams: player.audioTracks
        subtitleStreams: player.subtitleTracks
        currentVideoStream: player.activeVideoTrack
        currentAudioStream: player.activeAudioTrack
        currentSubtitleStream: player.activeSubtitleTrack
        chapters: player.chapters
        audioVisualizationEnabled: playerState.audioVisualizationEnabled

        Binding on subtitleStyle {
            value: player.subtitleStyle
            when: !playerState.applyingSubtitleStyle
        }

        onPlayPauseClicked: {
            if (player.playing) {
                player.pause()
            } else {
                if (player.mediaStatus === MediaPlayer.EndOfMedia) {
                    player.position = 0
                }
                player.play()
            }
        }

        onPreviousClicked: player.previous()

        onNextClicked: player.next()

        onSeekBackwardClicked: {
            var newPos = Math.max(0, player.position - 10000)
            player.position = newPos
        }

        onSeekForwardClicked: {
            var newPos = Math.min(player.duration, player.position + 10000)
            player.position = newPos
        }

        onPlaylistToggleClicked: playerState.togglePlaylist()

        onSettingsClicked: playerState.toggleSettings()

        onAudioVisualizationToggleClicked: playerState.audioVisualizationEnabled = !playerState.audioVisualizationEnabled

        onSubtitleStylePropChanged: playerState.applySubtitleStyleProp(prop, value)

        onResetSubtitleStyle: playerState.resetSubtitleStyle()

        onChapterSeekRequested: function(pos) {
            player.position = pos * 1000
        }

        onSpeedChange: function(speed) {
            player.playbackRate = speed
        }

        onRotationToggleClicked: {
            androidUtils.toggleOrientation()
        }

        Connections {
            target: playerControl.timeSlider
            function onSeek(pos) {
                player.position = pos * 1000
            }
        }
    }
}
