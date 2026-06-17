import QtQuick
import QtQuick.Layouts
import qz.theme
import qz.player
import QWindowKit
import "../controls"

// 播放器窗口标题栏，包含标题文本滚动和窗口控制按钮
Rectangle {
    id: root
    height: 50
    color: "transparent"

    property string titleText: ""
    property var windowAgent: null
    property var window: null
    property var mediaPlayer: null

    property bool isMiniMode: windowAgent ? windowAgent.miniMode : false
    property bool isAlwaysOnTop: windowAgent ? windowAgent.alwaysOnTop : false

    // 控制封面图的显示，标题栏通过位移隐藏时封面图需同步隐藏
    property bool showCoverArt: true

    // 时间显示
    property string _currentTime: Qt.formatTime(new Date(), "hh:mm")

    Timer {
        interval: 60000 // 每分钟更新一次
        running: true
        repeat: true
        onTriggered: root._currentTime = Qt.formatTime(new Date(), "hh:mm")
    }

    signal minimizeClicked()
    signal maximizeClicked()
    signal closeClicked()
    signal miniModeClicked()
    signal alwaysOnTopClicked()

    function registerWindowKitItems() {
        if (!windowAgent) return
        windowAgent.setHitTestVisible(alwaysOnTopButton)
        windowAgent.setHitTestVisible(miniModeButton)
        windowAgent.setSystemButton(WindowAgent.Minimize, minimizeButton)
        windowAgent.setSystemButton(WindowAgent.Maximize, maxButton)
        windowAgent.setSystemButton(WindowAgent.Close, closeButton)
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#aa000000" }
        GradientStop { position: 1.0; color: "transparent" }
    }

    CoverArtItem {
        id: coverArt
        mediaPlayer: root.mediaPlayer
        radius: 12
        maxDim: 100
        anchors{
            left: parent.left
            top: parent.top
            leftMargin: 10
            topMargin: 10
        }
        visible: !isEmpty && root.showCoverArt
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 4

        Item {
            Layout.preferredWidth: coverArt.width
        }

        Item {
            id: titleTextContainer
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.preferredHeight: titleTextImpl.implicitHeight
            Layout.rightMargin: 4
            clip: true

            property string titleText: root.titleText
            property int _scrollOffset: 0

            Text {
                id: titleTextImpl
                anchors.verticalCenter: parent.verticalCenter
                text: titleTextContainer.titleText
                font.pixelSize: 16
                font.bold: true
                color: Theme.textColor
                x: -titleTextContainer._scrollOffset
                onPaintedWidthChanged: titleTextContainer._checkScrollNeeded()
            }

            function _checkScrollNeeded() {
                if (titleTextImpl.paintedWidth > width) {
                    scrollTimer.start()
                } else {
                    scrollTimer.stop()
                    _scrollOffset = 0
                }
            }

            onTitleTextChanged: {
                _scrollOffset = 0
                _checkScrollNeeded()
            }

            onWidthChanged: _checkScrollNeeded()

            Timer {
                id: scrollTimer
                interval: 50
                repeat: true
                onTriggered: {
                    titleTextContainer._scrollOffset += 2
                    if (titleTextContainer._scrollOffset > titleTextImpl.paintedWidth + 50) {
                        titleTextContainer._scrollOffset = -titleTextContainer.width
                    }
                }
            }
        }

        Text {
            id: timeText
            Layout.alignment: Qt.AlignVCenter
            text: root._currentTime
            font.pixelSize: 16
            font.bold: true
            color: Theme.textColor
        }

        IconButton {
            id: alwaysOnTopButton
            visible: !root.isMiniMode
            active: root.isAlwaysOnTop
            iconAngle: root.isAlwaysOnTop ? -45 : 0
            source: "/res/icons/winAppBarIcon/QlementineIconsPin16.svg"
            onClicked: root.alwaysOnTopClicked()
        }

        IconButton {
            id: miniModeButton
            source: root.isMiniMode ?
                "/res/icons/winAppBarIcon/MaterialSymbolsPipExitOutlineRounded.svg" :
                "/res/icons/winAppBarIcon/MaterialSymbolsPipRounded.svg"
            onClicked: root.miniModeClicked()
        }

        IconButton {
            id: minimizeButton
            source: "/res/icons/winAppBarIcon/QlementineIconsWindowsMinimize16.svg"
            onClicked: root.minimizeClicked()
        }

        IconButton {
            id: maxButton
            visible: !root.isMiniMode
            source: root.window && root.window.visibility === Window.Maximized ?
                "/res/icons/winAppBarIcon/QlementineIconsWindowsMaximize16.svg" :
                "/res/icons/winAppBarIcon/QlementineIconsWindowsUnmaximize16.svg"
            onClicked: root.maximizeClicked()
        }

        IconButton {
            id: closeButton
            source: "/res/icons/winAppBarIcon/QlementineIconsClose16.svg"
            hoverColor: "#ff0000"
            onClicked: root.closeClicked()
        }
    }
}
