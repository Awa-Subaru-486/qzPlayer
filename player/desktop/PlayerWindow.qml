import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import qz.theme
import qz.player
import qz.multimedia
import QWindowKit
import qz.controls.md3
import "../controls"

Window {
    id: window
    width: 1280
    height: 720
    visible: true
    title: "AwaMediaHub Player"
    color: "#000000"

    property bool isShowControls: false
    property var previewFrames: []

    property int windowMode: 0

    readonly property int modeNormal: 0
    readonly property int modeMini: 1

    // 共享状态管理
    PlayerState {
        id: playerState
        mediaPlayer: player
    }

    // 播放列表数据模型
    PlaylistModel {
        id: playlistModel
    }

    WindowAgent {
        id: windowAgent
        onMiniModeChanged: {
            if (miniMode) {
                playerState.hideAllPanels()
                windowAgent.setTitleBar(miniOverlay)
                miniOverlay.registerHitTestItems()
                window.windowMode = window.modeMini
            } else {
                windowAgent.setTitleBar(titleBar)
                titleBar.registerWindowKitItems()
                window.windowMode = window.modeNormal
                updateWindowAspectRatio()
            }
        }
    }

    InputMonitor {
        id: mouseMonitor
    }

    HotkeyManager {
        id: hotkeyManager
        mediaPlayer: player
        window: window
        mode: HotkeyManager.WindowFocus
        Component.onCompleted: hotkeyManager.activate()
    }

    Component.onCompleted: {
        VulkanWindowInitializer.initialize(window)
        windowAgent.setup(window)
        windowAgent.setWindowAttribute("extra-margins", 0)
        windowAgent.setTitleBar(titleBar)
        titleBar.registerWindowKitItems()
        updateMonitorRegion()
        updateMiniModeThresholds()
    }

    function updateMiniModeThresholds() {
        windowAgent.miniModeThresholdWidth = playerControl.width + 40
        windowAgent.miniModeThresholdHeight = titleBar.height + playerControl.height + 80
    }

    function updateMonitorRegion() {
        mouseMonitor.setRegion(window.x, window.y, window.width, window.height)
    }

    onXChanged: updateMonitorRegion()
    onYChanged: updateMonitorRegion()
    onWidthChanged: updateMonitorRegion()
    onHeightChanged: updateMonitorRegion()

    Timer {
        id: hideTimer
        interval: 3000
        onTriggered: {
            if (window.windowMode === window.modeNormal && !windowAgent.miniMode) {
                if (window.isShowControls && !playerState.anyPanelVisible) {
                    hideControls()
                }
            } else {
                miniOverlay._controlsVisible = false
            }
        }
    }

    Connections {
        target: mouseMonitor

        function onInRegionChanged(isIn) {
            if (window.windowMode === window.modeNormal && !windowAgent.miniMode) {
                if (isIn) {
                    hideTimer.restart()
                    showControls()
                } else {
                    if (!playerState.anyPanelVisible) {
                        hideControls()
                    }
                }
            } else {
                if (isIn) {
                    miniOverlay._controlsVisible = true
                    hideTimer.restart()
                } else {
                    miniOverlay._controlsVisible = false
                }
            }
        }

        function onActivityInRegion() {
            hideTimer.restart()
        }
    }

    function showControls() {
        if (!window.isShowControls && window.windowMode === window.modeNormal) {
            window.isShowControls = true
        }
    }

    function hideControls() {
        if (window.isShowControls && window.windowMode === window.modeNormal) {
            window.isShowControls = false
        }
    }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: function(event) {
            if (playerState.anyPanelVisible) {
                playerState.hideAllPanels()
                event.accepted = true
            }
        }
    }

    MouseArea { id: cursorArea
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.NoButton
        cursorShape: (window.isShowControls || window.windowMode === window.modeMini || windowAgent.miniMode) ? Qt.ArrowCursor : Qt.BlankCursor
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
        playlistModel.addUrls(urls)
        player.addToPlaylist(urls)
    }

    function play() {
        player.play()
    }

    MediaPlayer {
        id: player
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
        volume: playerControl.volume / 100
        muted: playerControl.muted
    }

    AudioBufferOutput {
        id: audioBufferOutput
    }

    Item {
        id: videoContainer
        anchors {
            top: parent.top
            bottom: parent.bottom
        }
        x: (window.windowMode === window.modeNormal && !windowAgent.miniMode && playerState.chapterPanelVisible) ? chapterPanel.width + 10 : 0
        width: {
            if (window.windowMode === window.modeNormal && !windowAgent.miniMode) {
                if (playerState.chapterPanelVisible)
                    return window.width - chapterPanel.width - 10
                if (playerState.playlistPanelVisible)
                    return window.width - playlistPanel.width - 10
                if (playerState.settingsPanelVisible)
                    return window.width - settingsPanel.width - 10
            }
            return window.width
        }
        clip: true

        Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

        Rectangle {
            anchors.fill: parent
            color: "#000000"
            radius: (playerState.anyPanelVisible || window.windowMode === window.modeMini || windowAgent.miniMode) ? 8 : 0
            clip: true

            VideoOutput {
                id: videoOutput
                anchors.fill: parent
                fillMode: VideoOutput.PreserveAspectFit
                radius: (playerState.anyPanelVisible || window.windowMode === window.modeMini || windowAgent.miniMode) ? 8 : 0
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

            NotificationBar { }
        }

        MouseArea {
            anchors.fill: parent
            z: 200
            visible: playerState.anyPanelVisible
            onClicked: playerState.hideAllPanels()
        }
    }

    PlayerTitleBar {
        id: titleBar
        x: videoContainer.x
        width: videoContainer.width
        z: 10
        visible: window.windowMode === window.modeNormal && !windowAgent.miniMode
        y: window.isShowControls ? 0 : -height
        Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        titleText: window.title
        windowAgent: windowAgent
        window: window
        mediaPlayer: player
        showCoverArt: window.isShowControls

        onMinimizeClicked: window.showMinimized()
        onMaximizeClicked: {
            if (window.visibility === Window.Maximized) {
                window.showNormal()
            } else {
                window.showMaximized()
            }
        }
        onCloseClicked: window.close()
        onMiniModeClicked: windowAgent.miniMode = !windowAgent.miniMode
        onAlwaysOnTopClicked: windowAgent.alwaysOnTop = !windowAgent.alwaysOnTop
    }

    // 播放列表面板 - 使用共享 SlidePanel + PlaylistPanel
    SlidePanel {
        id: playlistPanel
        z: 5
        edge: SlidePanel.Edge.Right
        panelWidth: window.width * 0.4
        isOpen: playerState.playlistPanelVisible && window.windowMode === window.modeNormal && !windowAgent.miniMode

        PlaylistPanel {
            anchors.fill: parent
            mediaPlayer: player
            playlistModel: playlistModel
            showHoverEffect: true
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
        panelWidth: window.width * 0.45
        edge: SlidePanel.Edge.Right
        isOpen: playerState.settingsPanelVisible && window.windowMode === window.modeNormal && !windowAgent.miniMode

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
            stretchMode: playerState.stretchMode
            subtitleStyle: player.subtitleStyle
            audioVisualizationEnabled: playerState.audioVisualizationEnabled
            showStretchMode: true
            hotkeyMode: hotkeyManager.mode
            hotkeyManager: hotkeyManager
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
            onStretchModeSelected: function(mode) { playerState.stretchMode = mode }
            onSubtitleStylePropChanged: function(prop, value) { playerState.applySubtitleStyleProp(prop, value) }
            onResetSubtitleStyle: playerState.resetSubtitleStyle()
            onAudioVisualizationToggleClicked: playerState.audioVisualizationEnabled = !playerState.audioVisualizationEnabled
            onDecoderPrioritized: function(decoderId) { player.prioritizeDecoder(decoderId) }
            onDecoderDeprioritized: function(decoderId) { player.deprioritizeDecoder(decoderId) }
        }
    }

    // 章节面板 - 使用共享 SlidePanel + ChapterPanel
    SlidePanel {
        id: chapterPanel
        z: 5
        panelWidth: 260
        edge: SlidePanel.Edge.Left
        isOpen: playerState.chapterPanelVisible && window.windowMode === window.modeNormal && !windowAgent.miniMode

        ChapterPanel {
            anchors.fill: parent
            mediaPlayer: player
            delegateHeight: 36
        }
    }

    PlayerControl {
        id: playerControl
        anchors.horizontalCenter: videoContainer.horizontalCenter
        z: 10
        hotkeyManager: hotkeyManager
        visible: window.windowMode === window.modeNormal && !windowAgent.miniMode
        y: (window.windowMode === window.modeMini || windowAgent.miniMode) ? parent.height :
           playerState.anyPanelVisible ? parent.height :
           window.isShowControls ? parent.height - height - 16 : parent.height
        Behavior on y { NumberAnimation { duration: 250; easing.type: Easing.OutCubic } }

        duration: player.duration / 1000
        currentTime: player.position / 1000
        cacheValue: player.bufferProgress * player.duration / 1000
        isPause: !player.playing
        previewFrames: window.previewFrames
        mediaSource: player.source
        aspectRatioMode: playerState.aspectRatioMode
        stretchMode: playerState.stretchMode
        audioVisualizationEnabled: playerState.audioVisualizationEnabled
        settingsVisible: playerState.settingsPanelVisible

        videoStreams: player.videoTracks
        audioStreams: player.audioTracks
        subtitleStreams: player.subtitleTracks
        currentVideoStream: player.activeVideoTrack
        currentAudioStream: player.activeAudioTrack
        currentSubtitleStream: player.activeSubtitleTrack
        chapters: player.chapters

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

        onSubtitleToggleClicked: {
            if (player.activeSubtitleTrack >= 0) {
                player.setActiveSubtitleTrack(-1)
            } else {
                player.setActiveSubtitleTrack(0)
            }
        }

        onPlaylistToggleClicked: playerState.showPlaylist()

        onSettingsClicked: playerState.showSettings()

        onChapterToggleClicked: playerState.showChapter()

        onAudioVisualizationToggleClicked: playerState.audioVisualizationEnabled = !playerState.audioVisualizationEnabled

        onSubtitleStylePropChanged: playerState.applySubtitleStyleProp(prop, value)

        onResetSubtitleStyle: playerState.resetSubtitleStyle()

        onChapterSeekRequested: function(pos) {
            player.position = pos * 1000
        }

        onSpeedChange: function(speed) {
            player.playbackRate = speed
        }

        onFullscreenClicked: {
            if (window.visibility === Window.FullScreen) {
                window.showNormal()
            } else {
                window.showFullScreen()
            }
        }

        onAspectRatioModeChanged: {
            playerState.aspectRatioMode = playerControl.aspectRatioMode
            updateVideoOutputMode()
        }

        onStretchModeChanged: {
            playerState.stretchMode = playerControl.stretchMode
            updateVideoOutputMode()
        }

        Connections {
            target: playerControl.timeSlider
            function onSeek(pos) {
                player.position = pos * 1000
            }
        }
    }

    MiniModeOverlay {
        id: miniOverlay
        anchors.fill: videoContainer
        z: 11
        visible: window.windowMode === window.modeMini || windowAgent.miniMode

        windowAgent: windowAgent
        player: player
        duration: player.duration / 1000
        currentTime: player.position / 1000
        isPause: !player.playing

        onCloseClicked: window.close()
        onExitMiniModeClicked: windowAgent.miniMode = false
        onAlwaysOnTopClicked: windowAgent.alwaysOnTop = !windowAgent.alwaysOnTop
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
        onSeekBackwardClicked: {
            var newPos = Math.max(0, player.position - 10000)
            player.position = newPos
        }
        onSeekForwardClicked: {
            var newPos = Math.min(player.duration, player.position + 10000)
            player.position = newPos
        }
        onSeekRequested: function(pos) {
            player.position = pos * 1000
        }
    }

    function updateVideoOutputMode() {
        videoOutput.fillMode = VideoOutput.PreserveAspectFit
        updateWindowAspectRatio()
    }

    function updateWindowAspectRatio() {
        if (windowAgent.miniMode) return
        if (playerState.stretchMode === 1 && videoOutput.videoSink.videoSize.width > 0 && videoOutput.videoSink.videoSize.height > 0) {
            windowAgent.aspectRatio = videoOutput.videoSink.videoSize.width / videoOutput.videoSink.videoSize.height
            var w = window.width
            var h = Math.round(w / windowAgent.aspectRatio)
            window.width = w
            window.height = h
        } else {
            windowAgent.aspectRatio = 0
        }
    }

    Connections {
        target: videoOutput.videoSink
        function onVideoSizeChanged() {
            updateWindowAspectRatio()
            var size = videoOutput.videoSink.videoSize
            if (size.width > 0 && size.height > 0) {
                windowAgent.miniModeAspectRatio = size.width / size.height
            }
        }
    }

    Connections {
        target: playerControl
        function onWidthChanged() {
            windowAgent.miniModeThresholdWidth = playerControl.width + 40
        }
        function onHeightChanged() {
            windowAgent.miniModeThresholdHeight = titleBar.height + playerControl.height + 80
        }
    }

    Connections {
        target: titleBar
        function onHeightChanged() {
            windowAgent.miniModeThresholdHeight = titleBar.height + playerControl.height + 80
        }
    }
}
