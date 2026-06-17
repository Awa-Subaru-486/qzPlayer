import QtQuick
import QtQuick.Controls.Basic as BasicCtrl
import QtQuick.Layouts
import qz.theme
import qz.player

// 迷你模式下的播放器控制覆盖层
Item {
    id: root
    anchors.fill: parent

    property var windowAgent: null
    property var player: null
    property real duration: 0
    property real currentTime: 0
    property bool isPause: false
    property bool isAlwaysOnTop: windowAgent ? windowAgent.alwaysOnTop : false

    signal closeClicked()
    signal exitMiniModeClicked()
    signal playPauseClicked()
    signal seekBackwardClicked()
    signal seekForwardClicked()
    signal seekRequested(real pos)
    signal alwaysOnTopClicked()

    property bool _controlsVisible: false

    function registerHitTestItems() {
        if (!windowAgent) return
        windowAgent.setHitTestVisible(pinButton)
        windowAgent.setHitTestVisible(exitMiniButton)
        windowAgent.setHitTestVisible(closeButton)
        windowAgent.setHitTestVisible(rewindButton)
        windowAgent.setHitTestVisible(playPauseButton)
        windowAgent.setHitTestVisible(forwardButton)
        windowAgent.setHitTestVisible(progressBar)
    }

    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 36
        color: "transparent"

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#aa000000" }
            GradientStop { position: 1.0; color: "transparent" }
        }

        opacity: root._controlsVisible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        RowLayout {
            anchors.fill: parent
            anchors.rightMargin: 6
            anchors.topMargin: 4

            Item { Layout.fillWidth: true }

            IconButton {
                id: pinButton
                Layout.alignment: Qt.AlignVCenter
                active: root.isAlwaysOnTop
                iconAngle: root.isAlwaysOnTop ? -45 : 0
                source: "/res/icons/winAppBarIcon/QlementineIconsPin16.svg"
                onClicked: root.alwaysOnTopClicked()
            }

            IconButton {
                id: exitMiniButton
                Layout.alignment: Qt.AlignVCenter
                source: "/res/icons/winAppBarIcon/MaterialSymbolsPipExitOutlineRounded.svg"
                onClicked: root.exitMiniModeClicked()
            }

            IconButton {
                id: closeButton
                Layout.alignment: Qt.AlignVCenter
                source: "/res/icons/winAppBarIcon/QlementineIconsClose16.svg"
                hoverColor: "#ff0000"
                onClicked: root.closeClicked()
            }
        }
    }

    RowLayout {
        id: centerControls
        anchors.centerIn: parent
        spacing: 8
        visible: root._controlsVisible

        opacity: root._controlsVisible ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: 200 } }

        IconButton {
            id: rewindButton
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            source: "/res/icons/player/IconamoonPlayerStartFill.svg"
            onClicked: root.seekBackwardClicked()
        }

        IconButton {
            id: playPauseButton
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            source: root.isPause ? "/res/icons/player/IconamoonPlayerPlayFill.svg" : "/res/icons/player/IconamoonPlayerPauseFill.svg"
            onClicked: root.playPauseClicked()
        }

        IconButton {
            id: forwardButton
            Layout.preferredWidth: 36
            Layout.preferredHeight: 36
            source: "/res/icons/player/IconamoonPlayerEndFill.svg"
            onClicked: root.seekForwardClicked()
        }
    }

    BasicCtrl.ProgressBar {
        id: progressBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.bottomMargin: 10
        height: 4

        from: 0
        to: root.duration > 0 ? root.duration : 1
        value: root.currentTime

        background: Rectangle {
            implicitHeight: 4
            radius: 2
            color: Theme.isDark ? "#44ffffff" : "#33000000"
        }

        contentItem: Rectangle {
            implicitHeight: 4
            radius: 2
            width: progressBar.visualPosition * parent.width
            color: Theme.accentColor
        }

        opacity: root._controlsVisible ? 1 : 0.5
        Behavior on opacity { NumberAnimation { duration: 200 } }

        MouseArea {
            id: progressBarMouseArea
            anchors.fill: parent
            anchors.topMargin: -8
            anchors.bottomMargin: -8
            onClicked: function(mouse) {
                var pos = mouse.x / width * progressBar.to
                root.seekRequested(pos)
            }
        }
    }
}
