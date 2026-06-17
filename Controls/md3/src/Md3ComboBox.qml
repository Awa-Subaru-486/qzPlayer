// Md3ComboBox.qml - Material Design 3 风格下拉框组件
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
// Qt-Security score:significant reason:default

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Window
import QtQuick.Controls.impl
import QtQuick.Templates as T
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.ComboBox {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding,
        implicitIndicatorHeight + topPadding + bottomPadding)

    leftPadding: padding + (!control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)
    rightPadding: padding + (control.mirrored || !indicator || !indicator.visible ? 0 : indicator.width + spacing)

    // Removed Material.background and Material.foreground overrides to use Theme logic

    delegate: Md3MenuItem {
        required property var model
        required property int index

        width: ListView.view.width
        text: model[control.textRole]

        // MD3 Logic: Selected item uses Accent (Primary), others use OnSurface
        Material.foreground: control.currentIndex === index
            ? Theme.accentColor
            : (Theme.isDark ? "#E6E1E5" : "#1C1B1F")

        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
    }

    indicator: ColorImage {
        x: control.mirrored ? control.padding : control.width - width - control.padding
        y: control.topPadding + (control.availableHeight - height) / 2

        // MD3 Logic: OnSurfaceVariant (enabled) or OnSurfaceVariant at 38% (disabled)
        color: control.enabled
            ? (Theme.isDark ? "#CAC4D0" : "#49454F")
            : (Theme.isDark ? "#61CAC4D0" : "#6149454F")

        source: "qrc:/qt-project.org/imports/QtQuick/Controls/Material/images/drop-indicator.png"
    }

    contentItem: T.TextField {
        leftPadding: Material.textFieldHorizontalPadding
        topPadding: Material.textFieldVerticalPadding
        bottomPadding: Material.textFieldVerticalPadding

        text: control.editable ? control.editText : control.displayText

        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        selectByMouse: control.selectTextByMouse

        // MD3 Logic: OnSurface (enabled) or OnSurface at 38% (disabled)
        color: control.enabled
            ? (Theme.isDark ? "#E6E1E5" : "#1C1B1F")
            : (Theme.isDark ? "#61E6E1E5" : "#611C1B1F")

        // MD3 Logic: Primary (Accent)
        selectionColor: Theme.accentColor

        // MD3 Logic: OnPrimary (Contrast)
        selectedTextColor: {
            var c = Qt.color(Theme.accentColor);
            return (c.hslLightness < 0.5) ? "#FFFFFF" : "#000000";
        }

        verticalAlignment: Text.AlignVCenter

        cursorDelegate: CursorDelegate { }
    }

    background: MaterialTextContainer {
        implicitWidth: 120
        implicitHeight: control.Material.textFieldHeight

        // MD3 Logic: 
        // Disabled: Outline at 12% (approx #33...)
        // Default: Outline
        // Hovered: OnSurface
        // Focused: Accent (handled by focusedOutlineColor)
        outlineColor: !control.enabled
            ? (Theme.isDark ? "#33938F99" : "#3379747E")
            : (control.hovered
                ? (Theme.isDark ? "#E6E1E5" : "#1C1B1F")
                : (Theme.isDark ? "#938F99" : "#79747E"))

        focusedOutlineColor: Theme.accentColor
        controlHasActiveFocus: control.activeFocus
        controlHasText: true
        horizontalPadding: control.Material.textFieldHorizontalPadding
    }

    popup: T.Popup {
        y: control.editable ? control.height - 5 : 0
        width: control.width
        height: Math.min(contentItem.implicitHeight + verticalPadding * 2, control.Window.height - topMargin - bottomMargin)
        transformOrigin: Item.Top
        topMargin: 12
        bottomMargin: 12
        verticalPadding: 8

        Material.theme: control.Material.theme
        // Removed Material.accent and Material.primary to force usage of explicit colors in delegates

        enter: Transition {
            // grow_fade_in
            NumberAnimation { property: "scale"; from: 0.9; easing.type: Easing.OutQuint; duration: 220 }
            NumberAnimation { property: "opacity"; from: 0.0; easing.type: Easing.OutCubic; duration: 150 }
        }

        exit: Transition {
            // shrink_fade_out
            NumberAnimation { property: "scale"; to: 0.9; easing.type: Easing.OutQuint; duration: 220 }
            NumberAnimation { property: "opacity"; to: 0.0; easing.type: Easing.OutCubic; duration: 150 }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            T.ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            radius: 4
            // MD3 Logic: Surface color
            color: Theme.isDark ? "#1C1B1F" : "#FFFBFE"

            layer.enabled: control.enabled
            layer.effect: RoundedElevationEffect {
                elevation: 4
                roundedScale: Material.ExtraSmallScale
            }
        }
    }
}
