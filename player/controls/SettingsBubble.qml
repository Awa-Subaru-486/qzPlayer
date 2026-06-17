import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import qz.theme
import qz.controls.md3
import qz.player
import qz.multimedia

// 播放器设置气泡面板，包含视频/音频/字幕流选择和字幕样式配置
// 统一桌面端和移动端，通过 showStretchMode 控制是否显示窗口拉伸选项
Item { id: root

    property var videoStreams: []
    property var audioStreams: []
    property var subtitleStreams: []
    property int currentVideoStream: -1
    property int currentAudioStream: -1
    property int currentSubtitleStream: -1
    property bool isHdrEnabled: PlayerSet.hdrEnabled
    property bool isZeroCopyEnabled: PlayerSet.zeroCopyEnabled
    property bool isLowLatencyStreamingEnabled: PlayerSet.lowLatencyStreamingEnabled
    property int activeVideoDecoder: 0
    property var decoderPriorityList: []
    property int aspectRatioMode: 0
    property int stretchMode: 0
    property var subtitleStyle: ({})
    property bool audioVisualizationEnabled: false

    // 快捷键设置
    property int hotkeyMode: 0  // 0: Disabled, 1: WindowFocus, 2: Global
    property var hotkeyManager: null
    property int _hotkeyVersion: 0  // 递增计数器，用于刷新 UI 绑定
    property int _editingHotkeyAction: -1  // 当前正在编辑的快捷键 action，-1 表示无

    // 桌面端显示窗口拉伸选项，移动端不显示
    property bool showStretchMode: true

    // 从轨道信息构建显示名称
    function trackDisplayName(title, language, index) {
        if (title.length > 0 && language.length > 0)
            return title + " (" + language + ")"
        if (title.length > 0)
            return title
        if (language.length > 0)
            return qsTr("Track %1 (%2)").arg(index).arg(language)
        return qsTr("Track %1").arg(index)
    }

    signal hdrToggleClicked()
    signal zeroCopyToggleClicked()
    signal lowLatencyStreamingToggleClicked()
    signal videoStreamChanged(int index)
    signal audioStreamChanged(int index)
    signal subtitleStreamChanged(int index)
    signal aspectRatioSelected(int mode)
    signal stretchModeSelected(int mode)
    signal subtitleStylePropChanged(string prop, var value)
    signal resetSubtitleStyle()
    signal audioVisualizationToggleClicked()
    signal decoderPrioritized(int decoderId)
    signal decoderDeprioritized(int decoderId)

    component SectionTitle: Text {
        font.pixelSize: 13
        font.bold: true
        color: Theme.textColor
    }

    component SectionBox: Rectangle {
        radius: 8
        color: "transparent"
        border.color: Theme.isDark ? "#555555" : "#cccccc"
        border.width: 1
    }

    component SwitchRow: Row {
        height: 28
        spacing: 0

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: rowLabel
            font.pixelSize: 12
            color: Theme.textColor
        }

        Item { width: 1; height: 1; Layout.fillWidth: true }

        Md3Switch {
            anchors.verticalCenter: parent.verticalCenter
            scale: 0.7
            checked: rowChecked
            onCheckedChanged: {
                if (checked !== rowChecked) {
                    rowChecked = checked
                    rowToggled()
                }
            }
        }

        property string rowLabel
        property bool rowChecked
        signal rowToggled()
    }

    Item { id: settings_bubble
        anchors.fill: parent

        Item {
            anchors.fill: parent

            Connections {
                target: root.hotkeyManager
                function onHotkeyChanged() { root._hotkeyVersion++ }
                function onModeChanged() { root.hotkeyMode = root.hotkeyManager.mode }
            }

            Md3TabBar { id: settings_tabbar
                width: parent.width
                height: 40
                topPadding: 8
                background: Rectangle { color: "transparent" }

                Md3TabButton { text: qsTr("视频") }
                Md3TabButton { text: qsTr("音频") }
                Md3TabButton { text: qsTr("字幕") }
                Md3TabButton { text: qsTr("通用") }
            }

            StackLayout {
                width: parent.width
                anchors.top: settings_tabbar.bottom
                anchors.topMargin: 16
                anchors.bottom: parent.bottom
                currentIndex: settings_tabbar.currentIndex

                ScrollView { id: videoPage
                    clip: true
                    ScrollBar.horizontal: Md3ScrollBar {}

                    Column {
                        width: videoPage.width - 24
                        padding: 16
                        spacing: 14

                        Column {
                            width: parent.width - 32
                            visible: root.videoStreams.count > 0
                            spacing: 6

                            SectionTitle { text: qsTr("视频流") }

                            SectionBox {
                                width: parent.width
                                height: videoStreamGroup.height + 16

                                Column { id: videoStreamGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    Repeater {
                                        model: root.videoStreams
                                        Md3RadioButton {
                                            width: videoStreamGroup.width
                                            text: root.trackDisplayName(model.title, model.language, model.index)
                                            checked: model.index === root.currentVideoStream
                                            onCheckedChanged: {
                                                if (checked && model.index !== root.currentVideoStream) {
                                                    root.currentVideoStream = model.index
                                                    root.videoStreamChanged(model.index)
                                                }
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: videoStreamGroup.width
                                        text: qsTr("关闭")
                                        checked: root.currentVideoStream === -1
                                        onCheckedChanged: {
                                            if (checked && root.currentVideoStream !== -1) {
                                                root.currentVideoStream = -1
                                                root.videoStreamChanged(-1)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 窗口拉伸
                        Column {
                            width: parent.width - 32
                            visible: root.showStretchMode
                            spacing: 6

                            SectionTitle { text: qsTr("窗口拉伸") }

                            SectionBox {
                                width: parent.width
                                height: stretchGroup.height + 16

                                Column { id: stretchGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    Md3RadioButton {
                                        width: stretchGroup.width
                                        text: qsTr("自由") + " · " + qsTr("自由拉伸视频，黑边填充未使用区域")
                                        checked: root.stretchMode === 0
                                        onCheckedChanged: {
                                            if (checked && root.stretchMode !== 0) {
                                                root.stretchModeSelected(0)
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: stretchGroup.width
                                        text: qsTr("保持") + " · " + qsTr("自动调整窗口以适应视频比例，无黑边")
                                        checked: root.stretchMode === 1
                                        onCheckedChanged: {
                                            if (checked && root.stretchMode !== 1) {
                                                root.stretchModeSelected(1)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width - 32
                            spacing: 6

                            SectionTitle { text: qsTr("解码器") }

                            SectionBox {
                                width: parent.width
                                height: decoderGroup.height + 16

                                Column { id: decoderGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 6

                                    property var decoderNames: ({
                                        0: qsTr("软件解码"),
                                        1: "D3D11VA",
                                        2: "Vulkan",
                                        3: "MediaCodec"
                                    })

                                    // 解码器优先级列表，初始化后独立维护
                                    property var decoderOrder: Qt.platform.os === "windows" ? [2, 1, 0] : Qt.platform.os === "android" ? [3, 2, 0] : [2, 0]
                                    property int selectedDecoderIndex: -1

                                    Component.onCompleted: {
                                        var list = root.decoderPriorityList
                                        if (list && list.length > 0)
                                            decoderOrder = list.slice()
                                    }

                                    // 当前启用的解码器（来自播放器内核信号）
                                    Text {
                                        width: decoderGroup.width
                                        font.pixelSize: 12
                                        color: Theme.isDark ? "#aaaaaa" : "#666666"
                                        text: qsTr("当前：") + (decoderGroup.decoderNames[root.activeVideoDecoder] || qsTr("未知"))
                                    }

                                    // 左右布局：左侧列表 + 右侧上下按钮
                                    Row {
                                        width: decoderGroup.width
                                        spacing: 8

                                        // 解码器列表
                                        Column {
                                            width: parent.width - 40
                                            spacing: 2

                                            Repeater {
                                                model: decoderGroup.decoderOrder

                                                Rectangle {
                                                    width: decoderGroup.width - 40
                                                    height: 28
                                                    radius: 4
                                                    property int decoderId: modelData
                                                    property bool isSelected: index === decoderGroup.selectedDecoderIndex

                                                    color: isSelected ? (Theme.isDark ? Theme.accentColor : Theme.accentColor) :
                                                           dec_item_ma.containsMouse ? (Theme.isDark ? "#3a3a3a" : "#e8e8e8") : "transparent"
                                                    Behavior on color { ColorAnimation { duration: 100 } }

                                                    Text {
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        anchors.left: parent.left
                                                        anchors.leftMargin: 10
                                                        text: decoderGroup.decoderNames[decoderId] || decoderId
                                                        font.pixelSize: 12
                                                        color: isSelected ? "#ffffff" : Theme.textColor
                                                    }

                                                    MouseArea {
                                                        id: dec_item_ma
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        onClicked: decoderGroup.selectedDecoderIndex = index
                                                    }
                                                }
                                            }
                                        }

                                        // 上下按钮组
                                        Column {
                                            width: 32
                                            spacing: 4
                                            anchors.verticalCenter: parent.verticalCenter

                                            // 上移按钮
                                            IconButton {
                                                width: 32
                                                height: 28
                                                iconSize: 18
                                                source: "/res/icons/player/MaterialSymbolsTopPanelCloseRounded.svg"
                                                onClicked: {
                                                    if (decoderGroup.selectedDecoderIndex > 0) {
                                                        var order = decoderGroup.decoderOrder.slice()
                                                        var idx = decoderGroup.selectedDecoderIndex
                                                        var tmp = order[idx]
                                                        order[idx] = order[idx - 1]
                                                        order[idx - 1] = tmp
                                                        decoderGroup.decoderOrder = order
                                                        decoderGroup.selectedDecoderIndex = idx - 1
                                                        root.decoderPrioritized(order[idx - 1])
                                                    }
                                                }
                                            }

                                            // 下移按钮
                                            IconButton {
                                                width: 32
                                                height: 28
                                                iconSize: 18
                                                source: "/res/icons/player/MaterialSymbolsTopPanelOpenRounded.svg"
                                                onClicked: {
                                                    if (decoderGroup.selectedDecoderIndex >= 0 && decoderGroup.selectedDecoderIndex < decoderGroup.decoderOrder.length - 1) {
                                                        var order = decoderGroup.decoderOrder.slice()
                                                        var idx = decoderGroup.selectedDecoderIndex
                                                        var tmp = order[idx]
                                                        order[idx] = order[idx + 1]
                                                        order[idx + 1] = tmp
                                                        decoderGroup.decoderOrder = order
                                                        decoderGroup.selectedDecoderIndex = idx + 1
                                                        root.decoderDeprioritized(order[idx + 1])
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width - 32
                            spacing: 6

                            SectionTitle { text: qsTr("功能") }

                            SectionBox {
                                width: parent.width
                                height: funcGroup.height + 16

                                Column { id: funcGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    SwitchRow {
                                        width: funcGroup.width
                                        visible: Qt.platform.os !== "android"
                                        rowLabel: qsTr("HDR")
                                        rowChecked: root.isHdrEnabled
                                        onRowToggled: {
                                            PlayerSet.hdrEnabled = !PlayerSet.hdrEnabled
                                            root.hdrToggleClicked()
                                        }
                                    }

                                    SwitchRow {
                                        width: funcGroup.width
                                        visible: Qt.platform.os !== "android"
                                        rowLabel: qsTr("零拷贝")
                                        rowChecked: root.isZeroCopyEnabled
                                        onRowToggled: {
                                            PlayerSet.zeroCopyEnabled = !PlayerSet.zeroCopyEnabled
                                            root.zeroCopyToggleClicked()
                                        }
                                    }

                                    SwitchRow {
                                        width: funcGroup.width
                                        visible: Qt.platform.os !== "android"
                                        rowLabel: qsTr("低延迟流媒体")
                                        rowChecked: root.isLowLatencyStreamingEnabled
                                        onRowToggled: {
                                            PlayerSet.lowLatencyStreamingEnabled = !PlayerSet.lowLatencyStreamingEnabled
                                            root.lowLatencyStreamingToggleClicked()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                ScrollView { id: audioPage
                    clip: true
                    ScrollBar.horizontal: Md3ScrollBar {}

                    Column {
                        width: audioPage.width - 24
                        padding: 16
                        spacing: 14

                        Column {
                            width: parent.width - 32
                            visible: root.audioStreams.count > 0
                            spacing: 6

                            SectionTitle { text: qsTr("音频流") }

                            SectionBox {
                                width: parent.width
                                height: audioStreamGroup.height + 16

                                Column { id: audioStreamGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    Repeater {
                                        model: root.audioStreams
                                        Md3RadioButton {
                                            width: audioStreamGroup.width
                                            text: root.trackDisplayName(model.title, model.language, model.index)
                                            checked: model.index === root.currentAudioStream
                                            onCheckedChanged: {
                                                if (checked && model.index !== root.currentAudioStream) {
                                                    root.currentAudioStream = model.index
                                                    root.audioStreamChanged(model.index)
                                                }
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: audioStreamGroup.width
                                        text: qsTr("关闭")
                                        checked: root.currentAudioStream === -1
                                        onCheckedChanged: {
                                            if (checked && root.currentAudioStream !== -1) {
                                                root.currentAudioStream = -1
                                                root.audioStreamChanged(-1)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            visible: root.audioStreams.count === 0
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("无音频流")
                            font.pixelSize: 13
                            color: Theme.isDark ? "#666666" : "#999999"
                        }

                        Column {
                            width: parent.width - 32
                            spacing: 6

                            SectionTitle { text: qsTr("音频可视化") }

                            SectionBox {
                                width: parent.width
                                height: visualizerGroup.height + 16

                                Column { id: visualizerGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    SwitchRow {
                                        width: visualizerGroup.width
                                        rowLabel: qsTr("频谱可视化")
                                        rowChecked: root.audioVisualizationEnabled
                                        onRowToggled: root.audioVisualizationToggleClicked()
                                    }
                                }
                            }
                        }
                    }
                }

                ScrollView { id: subtitlePage
                    clip: true
                    ScrollBar.horizontal: Md3ScrollBar {}

                    Column {
                        width: subtitlePage.width - 24
                        padding: 16
                        spacing: 14

                        Column {
                            width: parent.width - 32
                            visible: root.subtitleStreams.count > 0
                            spacing: 6

                            SectionTitle { text: qsTr("字幕流") }

                            SectionBox {
                                width: parent.width
                                height: subtitleStreamGroup.height + 16

                                Column { id: subtitleStreamGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    Repeater {
                                        model: root.subtitleStreams
                                        Md3RadioButton {
                                            width: subtitleStreamGroup.width
                                            text: root.trackDisplayName(model.title, model.language, model.index)
                                            checked: model.index === root.currentSubtitleStream
                                            onCheckedChanged: {
                                                if (checked && model.index !== root.currentSubtitleStream) {
                                                    root.currentSubtitleStream = model.index
                                                    root.subtitleStreamChanged(model.index)
                                                }
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: subtitleStreamGroup.width
                                        text: qsTr("关闭")
                                        checked: root.currentSubtitleStream === -1
                                        onCheckedChanged: {
                                            if (checked && root.currentSubtitleStream !== -1) {
                                                root.currentSubtitleStream = -1
                                                root.subtitleStreamChanged(-1)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width - 32
                            visible: root.currentSubtitleStream >= 0
                            spacing: 6

                            SectionTitle { text: qsTr("字幕样式") }

                            SectionBox {
                                width: parent.width
                                height: subtitleStyleContent.height + 16

                                Column { id: subtitleStyleContent
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 8

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("大小")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 8; to: 48; stepSize: 1
                                            value: root.subtitleStyle.fontSize || 18
                                            onMoved: root.subtitleStylePropChanged("fontSize", value)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: (root.subtitleStyle.fontSize || 18) + "pt"
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("颜色")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Repeater {
                                            model: ["#ffffff", "#ffff00", "#00ff00", "#00ffff", "#ff8800", "#ff00ff"]
                                            Rectangle {
                                                width: 22; height: 22
                                                radius: 4
                                                color: modelData
                                                border.color: root.subtitleStyle.fontColor === modelData ? Theme.accentColor : (Theme.isDark ? "#555555" : "#cccccc")
                                                border.width: root.subtitleStyle.fontColor === modelData ? 2 : 1
                                                anchors.verticalCenter: parent.verticalCenter
                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: root.subtitleStylePropChanged("fontColor", modelData)
                                                }
                                            }
                                        }
                                    }

                                    SwitchRow {
                                        width: parent.width
                                        rowLabel: qsTr("粗体")
                                        rowChecked: root.subtitleStyle.bold || false
                                        onRowToggled: root.subtitleStylePropChanged("bold", !root.subtitleStyle.bold)
                                    }

                                    SwitchRow {
                                        width: parent.width
                                        rowLabel: qsTr("斜体")
                                        rowChecked: root.subtitleStyle.italic || false
                                        onRowToggled: root.subtitleStylePropChanged("italic", !root.subtitleStyle.italic)
                                    }

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("背景")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 0.0; to: 1.0; stepSize: 0.05
                                            value: root.subtitleStyle.backgroundOpacity !== undefined ? root.subtitleStyle.backgroundOpacity : 0.5
                                            onMoved: root.subtitleStylePropChanged("backgroundOpacity", value)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: Math.round((root.subtitleStyle.backgroundOpacity !== undefined ? root.subtitleStyle.backgroundOpacity : 0.5) * 100) + "%"
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("上边距")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 0.0; to: 0.3; stepSize: 0.01
                                            value: root.subtitleStyle.topMargin || 0.05
                                            onMoved: root.subtitleStylePropChanged("topMargin", value)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: Math.round((root.subtitleStyle.topMargin || 0.05) * 100) + "%"
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("下边距")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 0.0; to: 0.3; stepSize: 0.01
                                            value: root.subtitleStyle.bottomMargin || 0.05
                                            onMoved: root.subtitleStylePropChanged("bottomMargin", value)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: Math.round((root.subtitleStyle.bottomMargin || 0.05) * 100) + "%"
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("左边距")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 0.0; to: 0.3; stepSize: 0.01
                                            value: root.subtitleStyle.leftMargin || 0.05
                                            onMoved: root.subtitleStylePropChanged("leftMargin", value)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: Math.round((root.subtitleStyle.leftMargin || 0.05) * 100) + "%"
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Row {
                                        width: parent.width
                                        height: 28
                                        spacing: 8
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("右边距")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 0.0; to: 0.3; stepSize: 0.01
                                            value: root.subtitleStyle.rightMargin || 0.05
                                            onMoved: root.subtitleStylePropChanged("rightMargin", value)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: Math.round((root.subtitleStyle.rightMargin || 0.05) * 100) + "%"
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Md3Button {
                                        text: qsTr("恢复默认")
                                        onClicked: root.resetSubtitleStyle()
                                    }
                                }
                            }
                        }

                        Text {
                            visible: root.subtitleStreams.count === 0
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("无字幕流")
                            font.pixelSize: 13
                            color: Theme.isDark ? "#666666" : "#999999"
                        }
                    }
                }

                ScrollView { id: generalPage
                    clip: true
                    ScrollBar.horizontal: Md3ScrollBar {}

                    Column {
                        width: generalPage.width - 24
                        padding: 16
                        spacing: 14

                        Column {
                            width: parent.width - 32
                            spacing: 6

                            SectionTitle { text: qsTr("外观") }

                            SectionBox {
                                width: parent.width
                                height: themeGroup.height + 16

                                Column { id: themeGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    SwitchRow {
                                        width: themeGroup.width
                                        rowLabel: qsTr("跟随系统")
                                        rowChecked: PlayerSet.theme === ThemeType.System
                                        onRowToggled: {
                                            if (rowChecked) {
                                                PlayerSet.theme = ThemeType.System
                                                Theme.setTheme(ThemeType.System)
                                            } else {
                                                var fallback = PlayerSet.theme === ThemeType.Dark ? ThemeType.Dark : ThemeType.Light
                                                PlayerSet.theme = fallback
                                                Theme.setTheme(fallback)
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: themeGroup.width
                                        text: qsTr("浅色模式")
                                        visible: PlayerSet.theme !== ThemeType.System
                                        checked: PlayerSet.theme === ThemeType.Light
                                        onCheckedChanged: {
                                            if (checked) {
                                                PlayerSet.theme = ThemeType.Light
                                                Theme.setTheme(ThemeType.Light)
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: themeGroup.width
                                        text: qsTr("深色模式")
                                        visible: PlayerSet.theme !== ThemeType.System
                                        checked: PlayerSet.theme === ThemeType.Dark
                                        onCheckedChanged: {
                                            if (checked) {
                                                PlayerSet.theme = ThemeType.Dark
                                                Theme.setTheme(ThemeType.Dark)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width - 32
                            spacing: 6

                            SectionTitle { text: qsTr("片头片尾") }

                            SectionBox {
                                width: parent.width
                                height: skipGroup.height + 16

                                Column { id: skipGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 2

                                    property real _localOpening: PlayerSet.openingDuration
                                    property real _localEnding: PlayerSet.endingDuration
                                    property bool _hasChanges: _localOpening !== PlayerSet.openingDuration || _localEnding !== PlayerSet.endingDuration

                                    function _formatDuration(seconds) {
                                        var m = Math.floor(seconds / 60)
                                        var s = Math.floor(seconds % 60)
                                        return (m < 10 ? "0" : "") + m + ":" + (s < 10 ? "0" : "") + s
                                    }

                                    function _applyChanges() {
                                        PlayerSet.openingDuration = _localOpening
                                        PlayerSet.endingDuration = _localEnding
                                    }

                                    SwitchRow {
                                        width: skipGroup.width
                                        rowLabel: qsTr("跳过片头片尾")
                                        rowChecked: PlayerSet.skipEnabled
                                        onRowToggled: PlayerSet.skipEnabled = !PlayerSet.skipEnabled
                                    }

                                    Row {
                                        width: skipGroup.width
                                        height: 28
                                        spacing: 8
                                        opacity: PlayerSet.skipEnabled ? 1.0 : 0.4
                                        enabled: PlayerSet.skipEnabled

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("片头时长")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 30; to: 300; stepSize: 5
                                            value: skipGroup._localOpening
                                            onMoved: skipGroup._localOpening = value
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: skipGroup._formatDuration(skipGroup._localOpening)
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Row {
                                        width: skipGroup.width
                                        height: 28
                                        spacing: 8
                                        opacity: PlayerSet.skipEnabled ? 1.0 : 0.4
                                        enabled: PlayerSet.skipEnabled

                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: qsTr("片尾时长")
                                            font.pixelSize: 12
                                            color: Theme.textColor
                                            width: 50
                                        }
                                        Md3Slider {
                                            width: parent.width - 110
                                            anchors.verticalCenter: parent.verticalCenter
                                            from: 30; to: 300; stepSize: 5
                                            value: skipGroup._localEnding
                                            onMoved: skipGroup._localEnding = value
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: skipGroup._formatDuration(skipGroup._localEnding)
                                            font.pixelSize: 11
                                            color: Theme.isDark ? "#999999" : "#666666"
                                            width: 40
                                        }
                                    }

                                    Md3Button {
                                        visible: skipGroup._hasChanges
                                        text: qsTr("更新")
                                        onClicked: skipGroup._applyChanges()
                                    }
                                }
                            }
                        }

                        Column {
                            width: parent.width - 32
                            spacing: 6
                            visible: Qt.platform.os !== "android"

                            SectionTitle { text: qsTr("快捷键") }

                            SectionBox {
                                width: parent.width
                                height: hotkeyGroup.height + 16

                                Column { id: hotkeyGroup
                                    anchors {
                                        left: parent.left
                                        right: parent.right
                                        top: parent.top
                                        margins: 8
                                    }
                                    spacing: 8

                                    Text {
                                        text: qsTr("模式")
                                        font.pixelSize: 12
                                        color: Theme.textColor
                                    }

                                    Md3RadioButton {
                                        width: hotkeyGroup.width
                                        text: qsTr("禁用")
                                        checked: root.hotkeyMode === 0
                                        onCheckedChanged: {
                                            if (checked && root.hotkeyMode !== 0) {
                                                root.hotkeyMode = 0
                                                if (root.hotkeyManager) root.hotkeyManager.setMode(0)
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: hotkeyGroup.width
                                        text: qsTr("窗口焦点") + " · " + qsTr("仅在窗口获得焦点时生效")
                                        checked: root.hotkeyMode === 1
                                        onCheckedChanged: {
                                            if (checked && root.hotkeyMode !== 1) {
                                                root.hotkeyMode = 1
                                                if (root.hotkeyManager) root.hotkeyManager.setMode(1)
                                            }
                                        }
                                    }

                                    Md3RadioButton {
                                        width: hotkeyGroup.width
                                        text: qsTr("全局") + " · " + qsTr("全局快捷键，即使窗口未聚焦也生效")
                                        checked: root.hotkeyMode === 2
                                        onCheckedChanged: {
                                            if (checked && root.hotkeyMode !== 2) {
                                                root.hotkeyMode = 2
                                                if (root.hotkeyManager) root.hotkeyManager.setMode(2)
                                            }
                                        }
                                    }

                                    Item { width: 1; height: 8 }

                                    Text {
                                        text: qsTr("按键绑定")
                                        font.pixelSize: 12
                                        color: Theme.textColor
                                    }

                                    // 快捷键绑定列表
                                    Repeater {
                                        model: [
                                            { action: 0, label: qsTr("播放/暂停") },   // PlayPause
                                            { action: 1, label: qsTr("快进") },        // SeekForward
                                            { action: 2, label: qsTr("快退") },        // SeekBackward
                                            { action: 3, label: qsTr("停止") },        // Stop
                                            { action: 4, label: qsTr("加速") },        // SpeedUp
                                            { action: 5, label: qsTr("减速") },        // SpeedDown
                                            { action: 6, label: qsTr("音量增加") },    // VolumeUp
                                            { action: 7, label: qsTr("音量减少") },    // VolumeDown
                                            { action: 8, label: qsTr("静音") },        // Mute
                                            { action: 9, label: qsTr("全屏") }         // Fullscreen
                                        ]

                                        Row {
                                            width: hotkeyGroup.width
                                            height: 28
                                            spacing: 8

                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: modelData.label
                                                font.pixelSize: 12
                                                color: Theme.textColor
                                                width: 80
                                            }

                                            Rectangle {
                                                width: 100
                                                height: 24
                                                radius: 4
                                                color: Theme.isDark ? "#3a3a3a" : "#f0f0f0"
                                                border.color: root._editingHotkeyAction === modelData.action ? Theme.accentColor : (Theme.isDark ? "#555555" : "#cccccc")
                                                border.width: 1

                                                property string currentKey: root.hotkeyManager && root._hotkeyVersion >= 0 ? root.hotkeyManager.hotkey(modelData.action) : ""

                                                Text {
                                                    anchors.centerIn: parent
                                                    text: parent.currentKey || qsTr("未设置")
                                                    font.pixelSize: 11
                                                    color: parent.currentKey ? Theme.textColor : (Theme.isDark ? "#888888" : "#aaaaaa")
                                                }

                                                MouseArea { id: hotkeyInput
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor

                                                    onClicked: {
                                                        root._editingHotkeyAction = modelData.action
                                                        hotkeyCapture.action = modelData.action
                                                        hotkeyCapture.visible = true
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Md3Button {
                                        text: qsTr("恢复默认")
                                        onClicked: {
                                            if (root.hotkeyManager) {
                                                root.hotkeyManager.resetToDefaults()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // 快捷键捕获弹窗
            Rectangle { id: hotkeyCapture
                visible: false
                anchors.fill: parent
                color: Theme.isDark ? "#80000000" : "#80ffffff"

                property int action: 0
                property string pendingSequence: ""
                property string conflictAction: ""

                // action 枚举到名称的映射
                property var actionNames: [
                    qsTr("播放/暂停"), qsTr("快进"), qsTr("快退"), qsTr("停止"),
                    qsTr("加速"), qsTr("减速"), qsTr("音量增加"), qsTr("音量减少"),
                    qsTr("静音"), qsTr("全屏")
                ]

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        if (hotkeyCapture.conflictAction) return
                        hotkeyCapture.visible = false
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 280
                    height: conflictColumn.visible ? conflictColumn.height + 32 : captureColumn.height + 32
                    radius: 12
                    color: Theme.isDark ? "#2a2a2a" : "#ffffff"
                    border.color: Theme.isDark ? "#555555" : "#cccccc"
                    border.width: 1

                    Column { id: captureColumn
                        anchors.centerIn: parent
                        spacing: 12
                        visible: !hotkeyCapture.conflictAction

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("请按下快捷键")
                            font.pixelSize: 13
                            color: Theme.textColor
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("按 Esc 取消")
                            font.pixelSize: 11
                            color: Theme.isDark ? "#888888" : "#aaaaaa"
                        }
                    }

                    Column { id: conflictColumn
                        anchors.centerIn: parent
                        spacing: 10
                        visible: !!hotkeyCapture.conflictAction

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("快捷键冲突")
                            font.pixelSize: 14
                            font.bold: true
                            color: Theme.textColor
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("%1 已被 %2 使用").arg(hotkeyCapture.pendingSequence).arg(hotkeyCapture.conflictAction)
                            font.pixelSize: 12
                            color: Theme.isDark ? "#cccccc" : "#555555"
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: qsTr("是否覆盖？")
                            font.pixelSize: 12
                            color: Theme.isDark ? "#cccccc" : "#555555"
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 12

                            Md3Button {
                                text: qsTr("取消")
                                onClicked: hotkeyCapture.visible = false
                            }

                            Md3Button {
                                text: qsTr("确认")
                                highlighted: true
                                onClicked: {
                                    if (root.hotkeyManager) {
                                        root.hotkeyManager.setHotkey(hotkeyCapture.action, hotkeyCapture.pendingSequence)
                                    }
                                    hotkeyCapture.visible = false
                                }
                            }
                        }
                    }
                }

                function applyHotkey(sequence) {
                    if (!root.hotkeyManager) {
                        visible = false
                        return
                    }

                    // 检查冲突：遍历所有 action，看是否有其他 action 已绑定此按键
                    let conflictIdx = -1
                    for (let i = 0; i < 10; i++) {
                        if (i === action) continue
                        let existing = root.hotkeyManager.hotkey(i)
                        if (existing === sequence) {
                            conflictIdx = i
                            break
                        }
                    }

                    if (conflictIdx >= 0) {
                        pendingSequence = sequence
                        conflictAction = actionNames[conflictIdx]
                    } else {
                        root.hotkeyManager.setHotkey(action, sequence)
                        visible = false
                    }
                }

                Keys.onPressed: function(event) {
                    if (!visible) {
                        event.accepted = true
                        return
                    }

                    // 冲突确认状态下不捕获新按键
                    if (conflictAction) {
                        if (event.key === Qt.Key_Escape) {
                            visible = false
                        }
                        event.accepted = true
                        return
                    }

                    if (event.key === Qt.Key_Escape) {
                        visible = false
                        event.accepted = true
                        return
                    }

                    // 忽略单独的修饰键
                    if (event.key === Qt.Key_Control || event.key === Qt.Key_Alt || event.key === Qt.Key_Shift) {
                        event.accepted = true
                        return
                    }

                    let modifiers = []
                    if (event.modifiers & Qt.ControlModifier) modifiers.push("Ctrl")
                    if (event.modifiers & Qt.AltModifier) modifiers.push("Alt")
                    if (event.modifiers & Qt.ShiftModifier) modifiers.push("Shift")

                    let keyText = ""
                    switch(event.key) {
                        case Qt.Key_Space: keyText = "Space"; break
                        case Qt.Key_Return: case Qt.Key_Enter: keyText = "Return"; break
                        case Qt.Key_Backspace: keyText = "Backspace"; break
                        case Qt.Key_Tab: keyText = "Tab"; break
                        case Qt.Key_Left: keyText = "Left"; break
                        case Qt.Key_Right: keyText = "Right"; break
                        case Qt.Key_Up: keyText = "Up"; break
                        case Qt.Key_Down: keyText = "Down"; break
                        case Qt.Key_Home: keyText = "Home"; break
                        case Qt.Key_End: keyText = "End"; break
                        case Qt.Key_PageUp: keyText = "PageUp"; break
                        case Qt.Key_PageDown: keyText = "PageDown"; break
                        case Qt.Key_Insert: keyText = "Insert"; break
                        case Qt.Key_Delete: keyText = "Delete"; break
                        case Qt.Key_F1: case Qt.Key_F2: case Qt.Key_F3: case Qt.Key_F4:
                        case Qt.Key_F5: case Qt.Key_F6: case Qt.Key_F7: case Qt.Key_F8:
                        case Qt.Key_F9: case Qt.Key_F10: case Qt.Key_F11: case Qt.Key_F12:
                            keyText = "F" + (event.key - Qt.Key_F1 + 1)
                            break
                        default:
                            if (event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
                                keyText = String.fromCharCode(event.key)
                            } else if (event.key >= Qt.Key_0 && event.key <= Qt.Key_9) {
                                keyText = String.fromCharCode(event.key)
                            }
                    }

                    if (keyText) {
                        modifiers.push(keyText)
                        let sequence = modifiers.join("+")
                        applyHotkey(sequence)
                    }
                    event.accepted = true
                }

                onVisibleChanged: {
                    if (visible) {
                        conflictAction = ""
                        pendingSequence = ""
                        forceActiveFocus()
                        if (root.hotkeyManager) root.hotkeyManager.deactivate()
                    } else {
                        root._editingHotkeyAction = -1
                        conflictAction = ""
                        pendingSequence = ""
                        focus = false
                        if (root.hotkeyManager) root.hotkeyManager.activate()
                    }
                }
            }
        }
    }
}
