// Md3Popup.qml - Material Design 3 风格弹窗组件
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.Popup {
    id: control

    clip: true
    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)

    padding: 12

    Material.elevation: 4
    Material.roundedScale: Material.ExtraSmallScale

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

    background: Rectangle {
        // FullScale doesn't make sense for Popup.
        radius: Theme.roundedScale
        // MD3 Surface Container Low Color (用于区分背景和遮罩)
        color: Theme.isDark ? "#49454F" : "#F3F6F8"

        layer.enabled: control.Material.elevation > 0
        layer.effect: RoundedElevationEffect {
            elevation: control.Material.elevation
            roundedScale: Theme.roundedScale
        }
    }

    T.Overlay.modal: Rectangle {
        // MD3 Scrim Color
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.6 : 0.32)
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }

    T.Overlay.modeless: Rectangle {
        // MD3 Scrim Color
        color: Qt.rgba(0, 0, 0, Theme.isDark ? 0.6 : 0.32)
        Behavior on opacity { NumberAnimation { duration: 150 } }
    }
}
