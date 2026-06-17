import QtQuick
import QtQuick.Layouts
import qz.theme
import qz.player
import "../controls"

// 移动端播放器窗口标题栏，包含标题文本滚动、封面图、小窗按钮和关闭按钮
Rectangle {
    id: root
    height: 50
    color: "transparent"

    property string titleText: ""
    property var mediaPlayer: null
    property var chapters: []

    signal closeClicked()
    signal miniModeClicked()
    signal rotationToggleClicked()
    signal settingsClicked()
    signal chapterClicked()

    // 时间显示
    property string _currentTime: Qt.formatTime(new Date(), "hh:mm")

    Timer {
        interval: 60000 // 每分钟更新一次
        running: true
        repeat: true
        onTriggered: root._currentTime = Qt.formatTime(new Date(), "hh:mm")
    }

    // 电量相关
    property int _batteryLevel: androidUtils.batteryLevel()
    property bool _batteryCharging: androidUtils.isBatteryCharging()

    AndroidUtils { id: androidUtils }

    // 监听电池状态变化信号
    Connections {
        target: androidUtils
        function onBatteryStatusChanged() {
            root._batteryLevel = androidUtils.batteryLevel()
            root._batteryCharging = androidUtils.isBatteryCharging()
        }
    }

    function _batteryIconSource() {
        if (root._batteryCharging)
            return "/res/icons/player/MingcuteBatteryChargingLine.svg"
        var level = root._batteryLevel
        if (level <= 25) return "/res/icons/player/MingcuteBattery1Line.svg"
        if (level <= 50) return "/res/icons/player/MingcuteBattery2Line.svg"
        if (level <= 75) return "/res/icons/player/MingcuteBattery3Line.svg"
        return "/res/icons/player/MingcuteBattery4Line.svg"
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: "#aa000000" }
        GradientStop { position: 1.0; color: "transparent" }
    }

    CoverArtItem {
        mediaPlayer: root.mediaPlayer
        radius: 4
        maxDim: root.height - 10
        id: coverArt
        anchors{
            left: parent.left
            top: parent.top
            leftMargin: 10
            topMargin: 10
        }
        visible: !isEmpty
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
            clip: true

            Text {
                id: titleTextImpl
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                text: root.titleText
                font.pixelSize: 16
                font.bold: true
                color: Theme.textColor
                elide: Text.ElideRight
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

        SvgRenderer {
            id: batteryIcon
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            width: 28
            height: 28
            paintedSize: 28
            source: root._batteryIconSource()
            color: Theme.textColor
        }

        IconButton {
            id: settingsButton
            source: "/res/icons/player/IconamoonSettingsFill.svg"
            onClicked: root.settingsClicked()
        }

        IconButton {
            id: chapterButton
            visible: root.chapters.length > 0
            source: "/res/icons/player/GgEreader.svg"
            onClicked: root.chapterClicked()
        }

        IconButton {
            id: miniModeButton
            source: "/res/icons/winAppBarIcon/MaterialSymbolsPipRounded.svg"
            onClicked: root.miniModeClicked()
        }

        IconButton {
            id: rotationButton
            implicitWidth: 28
            implicitHeight: 28
            source: "/res/icons/player/MaterialSymbolsScreenRotationRounded.svg"
            onClicked: root.rotationToggleClicked()
        }

        IconButton {
            id: closeButton
            implicitWidth: 36
            implicitHeight: 36
            source: "/res/icons/winAppBarIcon/QlementineIconsClose16.svg"
            hoverColor: "#ff0000"
            onClicked: root.closeClicked()
        }
    }
}
