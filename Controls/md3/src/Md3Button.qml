// Md3Button.qml - Material Design 3 风格按钮组件
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.Button { id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)

    topInset: 6
    bottomInset: 6
    verticalPadding: Material.buttonVerticalPadding
    leftPadding: Material.buttonLeftPadding(flat, hasIcon && (display !== AbstractButton.TextOnly))
    rightPadding: Material.buttonRightPadding(flat, hasIcon && (display !== AbstractButton.IconOnly),
        (text !== "") && (display !== AbstractButton.IconOnly))
    spacing: 8

    icon.width: 24
    icon.height: 24
    icon.color: !control.enabled ? (Theme.isDark ? "#60FFFFFF" : "#61000000") :
        (control.flat ? Theme.accentColor : "#FFFFFF")

    readonly property bool hasIcon: icon.name.length > 0 || icon.source.toString().length > 0

    // 默认值为 "transparent"，如果未设置则使用原有逻辑
    property color customBgColor: "transparent"

    Material.elevation: control.down ? 8 : 2
    Material.roundedScale: Material.FullScale

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font: control.font
        color: !control.enabled ? (Theme.isDark ? "#60FFFFFF" : "#61000000") :
            (control.flat ? Theme.accentColor : "#FFFFFF")
    }

    background: Rectangle {
        implicitWidth: 64
        implicitHeight: control.Material.buttonHeight

        radius: Theme.roundedScale === Material.FullScale ? height / 2 : Theme.roundedScale

        // --- 修改开始：背景颜色逻辑更新 ---
        color: {
            // 1. 禁用状态颜色 (保持原有逻辑)
            if (!control.enabled) {
                return Theme.isDark ? "#49454F" : "#E7E0EC"
            }

            // 2. 如果设置了自定义背景颜色且不为完全透明，则优先使用自定义颜色
            // 这样你可以覆盖 flat 属性或者默认的灰色
            if (control.customBgColor.a > 0) {
                return control.customBgColor
            }

            // 3. 扁平化按钮逻辑
            if (control.flat) {
                return control.highlighted ? (Theme.isDark ? "#1FFFFFFF" : "#1A000000") : "transparent"
            }

            // 4. 默认实心按钮颜色
            return Theme.isDark ? "#474747" : "#D6D7D7"
        }
        // --- 修改结束 ---

        // The layer is disabled when the button color is transparent so you can do
        // Material.background: "transparent" and get a proper flat button without needing
        // to set Material.elevation as well
        layer.enabled: control.enabled && color.a > 0 && !control.flat
        layer.effect: RoundedElevationEffect {
            elevation: control.Material.elevation
            roundedScale: control.background.radius
        }

        Ripple {
            clip: true
            clipRadius: parent.radius
            width: parent.width
            height: parent.height
            pressed: control.pressed
            anchor: control
            active: enabled && (control.down || control.visualFocus || control.hovered)
            color: control.flat ? Qt.rgba(Theme.accentColor.r, Theme.accentColor.g, Theme.accentColor.b, 0.12) :
                Qt.rgba(1, 1, 1, 0.12)
        }
    }
}
