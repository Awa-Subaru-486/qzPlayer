// Md3SliderHandle.qml - Material Design 3 滑块手柄内部组件
// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

import QtQuick
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

Item {
    id: root
    implicitWidth: initialSize
    implicitHeight: initialSize

    property real value: 0
    property bool handleHasFocus: false
    property bool handlePressed: false
    property bool handleHovered: false
    readonly property int initialSize: 13
    readonly property var control: parent

    Rectangle {
        id: handleRect
        width: parent.width
        height: parent.height
        radius: width / 2
        // MD3 标准：启用时使用主色调，禁用时使用 On Surface 颜色的 38% 不透明度
        color: root.control
            ? root.control.enabled
                ? Theme.accentColor
                : (Theme.isDark ? "#60FFFFFF" : "#61000000")
            : "transparent"
    }

    Ripple {
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 22; height: 22
        pressed: root.handlePressed
        active: root.handlePressed || root.handleHasFocus || (enabled && root.handleHovered)
        // 这里使用基于主题的黑白半透明颜色模拟，确保不依赖 control.Material
        color: Color.transparent(Theme.accentColor, 0.12)
    }
}
