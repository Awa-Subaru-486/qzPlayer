import QtQuick
import qz.theme

// 可复用滑动面板组件
// 支持从左侧、右侧或底部滑入/滑出，带动画效果
// 用于播放列表、设置、章节等面板
Rectangle {
    id: panel

    enum Edge { Left, Right, Bottom }

    // 从哪一侧滑入
    required property int edge

    // 面板是否打开
    property bool isOpen: false

    // 面板尺寸：Left/Right 时为宽度，Bottom 时为高度
    property real panelWidth: 260

    radius: 12
    color: Theme.isDark ? "#dd1a1a1a" : "#ddf0f0f0"
    border.color: Theme.isDark ? "#33ffffff" : "#22000000"
    border.width: 1
    clip: true
    visible: false

    // Left/Right: 全高，可变宽度；Bottom: 全宽，可变高度
    width: panel.edge === SlidePanel.Edge.Bottom ? parent.width - 10 : panelWidth
    height: panel.edge === SlidePanel.Edge.Bottom ? panelWidth : parent.height - 20

    // 根据 edge 计算隐藏/显示位置
    property real _hiddenX: {
        if (panel.edge === SlidePanel.Edge.Right) return parent.width + 5
        if (panel.edge === SlidePanel.Edge.Left) return -width - 5
        return 5 // Bottom
    }
    property real _visibleX: {
        if (panel.edge === SlidePanel.Edge.Right) return parent.width - width - 5
        if (panel.edge === SlidePanel.Edge.Left) return 5
        return 5 // Bottom
    }

    property real _hiddenY: {
        if (panel.edge === SlidePanel.Edge.Bottom) return parent.height + 5
        return 10 // Left/Right
    }
    property real _visibleY: {
        if (panel.edge === SlidePanel.Edge.Bottom) return parent.height - height - 5
        return 10 // Left/Right
    }

    x: _hiddenX
    y: _hiddenY

    states: [
        State {
            name: "visible"
            when: isOpen
            PropertyChanges { target: panel; x: panel._visibleX; y: panel._visibleY; visible: true }
        },
        State {
            name: "hidden"
            PropertyChanges { target: panel; x: panel._hiddenX; y: panel._hiddenY; visible: false }
        }
    ]

    transitions: [
        Transition {
            to: "visible"
            SequentialAnimation {
                PropertyAction { property: "visible"; value: true }
                NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.OutCubic }
            }
        },
        Transition {
            to: "hidden"
            SequentialAnimation {
                NumberAnimation { properties: "x,y"; duration: 200; easing.type: Easing.InCubic }
                PropertyAction { property: "visible"; value: false }
            }
        }
    ]

    // 默认数据容器 - 子组件直接填充面板区域
    default property alias content: contentContainer.data

    Item {
        id: contentContainer
        anchors.fill: parent
    }
}
