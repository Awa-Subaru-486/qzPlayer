import QtQuick
import qz.player

// 播放器共享状态管理对象
// 集中管理播放器设置、面板可见性、字幕样式操作等跨平台共享状态
QtObject {
    id: state

    // === 播放器设置状态 ===
    property int aspectRatioMode: 0
    property int stretchMode: 0
    property bool audioVisualizationEnabled: false
    property int savedVideoTrack: 0

    // === 面板可见性状态 ===
    property bool playlistPanelVisible: false
    property bool settingsPanelVisible: false
    property bool chapterPanelVisible: false

    // === 字幕样式守卫 ===
    property bool applyingSubtitleStyle: false

    // === 便捷计算属性 ===
    readonly property bool anyPanelVisible: playlistPanelVisible || settingsPanelVisible || chapterPanelVisible

    // === MediaPlayer 引用（由 PlayerWindow 设置） ===
    property var mediaPlayer: null

    // === 音频可视化切换 ===
    onAudioVisualizationEnabledChanged: {
        // 音频可视化功能待重新连接
    }

    // === 面板互斥切换方法 ===
    function showPlaylist() {
        settingsPanelVisible = false
        chapterPanelVisible = false
        playlistPanelVisible = true
    }

    function showSettings() {
        playlistPanelVisible = false
        chapterPanelVisible = false
        settingsPanelVisible = true
    }

    function showChapter() {
        playlistPanelVisible = false
        settingsPanelVisible = false
        chapterPanelVisible = true
    }

    function togglePlaylist() {
        if (!playlistPanelVisible) showPlaylist()
        else playlistPanelVisible = false
    }

    function toggleSettings() {
        if (!settingsPanelVisible) showSettings()
        else settingsPanelVisible = false
    }

    function toggleChapter() {
        if (!chapterPanelVisible) showChapter()
        else chapterPanelVisible = false
    }

    function hideAllPanels() {
        playlistPanelVisible = false
        settingsPanelVisible = false
        chapterPanelVisible = false
    }

    // === 字幕样式操作（集中管理，消除重复） ===
    function applySubtitleStyleProp(prop, value) {
        if (applyingSubtitleStyle || !mediaPlayer) return
        applyingSubtitleStyle = true
        var props = {}
        props[prop] = value
        mediaPlayer.applySubtitleStyle(props)
        Qt.callLater(function() { applyingSubtitleStyle = false })
    }

    function resetSubtitleStyle() {
        if (applyingSubtitleStyle || !mediaPlayer) return
        applyingSubtitleStyle = true
        mediaPlayer.applySubtitleStyle({
            fontSize: 18,
            fontColor: "#ffffff",
            bold: false,
            italic: false,
            backgroundColor: "#000000",
            backgroundOpacity: 0.5,
            cornerRadius: 0.1,
            topMargin: 0.05,
            bottomMargin: 0.05,
            leftMargin: 0.05,
            rightMargin: 0.05
        })
        Qt.callLater(function() { applyingSubtitleStyle = false })
    }
}
