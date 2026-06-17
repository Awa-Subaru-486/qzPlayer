// Md3CheckIndicator.qml - Material Design 3 复选框指示器内部组件
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import qz.theme

Rectangle {
    id: indicatorItem
    implicitWidth: 18
    implicitHeight: 18
    color: "transparent"

    // 根据 MD3 规范定义颜色
    // 1. 禁用状态：onSurface @ 38% opacity
    // 2. 选中状态：accentColor
    // 3. 未选中状态：outline (近似处理)
    border.color: {
        if (!control.enabled) {
            // MD3: 浅色模式 #1F000000 (12%), 深色模式 #1FFFFFFF
            // 这里使用 38% 不透明度以符合禁用态标准
            return Theme.isDark ? "#61FFFFFF" : "#61000000"
        }
        if (checkState !== Qt.Unchecked) {
            return Theme.accentColor
        }
        // 未选中启用状态：MD3 Outline
        // 浅色模式约为 54% 黑色，深色模式约为 70% 白色
        return Theme.isDark ? "#B3FFFFFF" : "#8A000000"
    }

    border.width: checkState !== Qt.Unchecked ? width / 2 : 2
    radius: 2

    property Item control
    property int checkState: control.checkState

    Behavior on border.width {
        NumberAnimation {
            duration: 100
            easing.type: Easing.OutCubic
        }
    }

    Behavior on border.color {
        ColorAnimation {
            duration: 100
            easing.type: Easing.OutCubic
        }
    }

    // 注意：如果您完全移除了 QtQuick.Controls.Material 插件，
    // 下面的图像路径 "qrc:/qt-project.org/..." 可能会无法加载。
    // 如果该文件在您的资源系统中不存在，请替换为本地资源路径。
    Image {
        id: checkImage
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 14
        height: 14
        source: "qrc:/qt-project.org/imports/QtQuick/Controls/Material/images/check.png"
        fillMode: Image.PreserveAspectFit

        scale: indicatorItem.checkState === Qt.Checked ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 100 } }
    }

    Rectangle {
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 12
        height: 3
        // 确保部分选中时的横线是白色的，与选中背景搭配
        color: "white"

        scale: indicatorItem.checkState === Qt.PartiallyChecked ? 1 : 0
        Behavior on scale { NumberAnimation { duration: 100 } }
    }

    states: [
        State {
            name: "checked"
            when: indicatorItem.checkState === Qt.Checked
        },
        State {
            name: "partiallychecked"
            when: indicatorItem.checkState === Qt.PartiallyChecked
        }
    ]

    transitions: Transition {
        SequentialAnimation {
            NumberAnimation {
                target: indicatorItem
                property: "scale"
                // Go down 2 pixels in size.
                to: 1 - 2 / indicatorItem.width
                duration: 120
            }
            NumberAnimation {
                target: indicatorItem
                property: "scale"
                to: 1
                duration: 120
            }
        }
    }
}
