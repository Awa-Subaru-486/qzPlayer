// Md3Menu.qml - Material Design 3 风格菜单组件
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.Menu { id: control
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)

    margins: 0
    verticalPadding: 8

    transformOrigin: !cascade ? Item.Top : (mirrored ? Item.TopRight : Item.TopLeft)

    Material.elevation: 4
    Material.roundedScale: Material.ExtraSmallScale

    delegate: MenuItem { }

    enter: Transition {
        // grow_fade_in
        NumberAnimation { property: "scale"; from: 0.9; to: 1.0; easing.type: Easing.OutQuint; duration: 220 }
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; easing.type: Easing.OutCubic; duration: 150 }
    }

    exit: Transition {
        // shrink_fade_out
        NumberAnimation { property: "scale"; from: 1.0; to: 0.9; easing.type: Easing.OutQuint; duration: 220 }
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; easing.type: Easing.OutCubic; duration: 150 }
    }

    contentItem: ListView {
        implicitHeight: contentHeight

        model: control.contentModel
        interactive: Window.window
            ? contentHeight + control.topPadding + control.bottomPadding > control.height
            : false
        clip: true
        currentIndex: control.currentIndex

        ScrollIndicator.vertical: ScrollIndicator {}
    }

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: control.Material.menuItemHeight

        radius: control.Material.roundedScale

        // MD3 风格的菜单背景：使用 Surface 容器颜色，与遮罩区分开。
        // 浅色用略灰的 surface 容器色，深色用浅灰容器色，以与页面背景形成层级。
        color: Theme.isDark ? "#313033" : "#F3F6F8"

        layer.enabled: control.Material.elevation > 0
        layer.effect: RoundedElevationEffect {
            elevation: control.Material.elevation
            roundedScale: Theme.roundedScale
        }
    }

    T.Overlay.modal: Rectangle {
        // MD3 模态遮罩色（scrim）：浅色 32%、深色 60% 的黑色叠加。
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.60 : 0.32)
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    T.Overlay.modeless: Rectangle {
        // MD3 非模态遮罩色，复用同一套 scrim 规范。
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.60 : 0.32)
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }
}
