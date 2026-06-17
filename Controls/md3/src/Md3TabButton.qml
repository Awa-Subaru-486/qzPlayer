// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.TabButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)

    padding: 12
    spacing: 6

    icon.width: 24
    icon.height: 24
    icon.color: !enabled ? Theme.textColor : down || checked ? Theme.accentColor : Theme.textColor

    contentItem: IconLabel {
        spacing: control.spacing
        mirrored: control.mirrored
        display: control.display

        icon: control.icon
        text: control.text
        font: control.font
        color: !control.enabled ? Theme.textColor : control.down || control.checked ? Theme.accentColor : Theme.textColor
    }

    background: Ripple {
        implicitHeight: control.Material.touchTarget

        clip: true
        pressed: control.pressed
        anchor: control
        active: enabled && (control.down || control.visualFocus || control.hovered)
        color: control.Material.rippleColor
    }
}
