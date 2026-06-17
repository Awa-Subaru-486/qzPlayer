// Md3TextField.qml - Material Design 3 风格文本输入框组件
// Copyright (C) 2017 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.impl
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.TextField {
    id: control

    // 1. 定义左右 Item 属性
    property Item leftItem
    property Item rightItem

    implicitWidth: implicitBackgroundWidth + leftInset + rightInset
        || Math.max(contentWidth, placeholder.implicitWidth) + leftPadding + rightPadding
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        contentHeight + topPadding + bottomPadding)

    topInset: clip ? placeholder.largestHeight / 2 : 0

    // 2. 动态 Padding
    // 如果外部定义了 leftItem 并设置了 width (如 32)，这里会直接加上 32 和间距 8
    // 这样文字输入区域就会自动避让
    readonly property real leftItemWidth: leftItem ? (leftItem.width || leftItem.implicitWidth || 0) : 0
    readonly property real rightItemWidth: rightItem ? (rightItem.width || rightItem.implicitWidth || 0) : 0
    readonly property real itemSpacing: 8

    leftPadding: Material.textFieldHorizontalPadding + leftItemWidth + (leftItem ? itemSpacing : 0)
    rightPadding: Material.textFieldHorizontalPadding + rightItemWidth + (rightItem ? itemSpacing : 0)

    topPadding: Material.containerStyle === Material.Filled
        ? placeholderText.length > 0 && (activeFocus || length > 0)
            ? Material.textFieldVerticalPadding + placeholder.largestHeight
            : Material.textFieldVerticalPadding
        : Material.textFieldVerticalPadding + topInset
    bottomPadding: Material.textFieldVerticalPadding

    // Color Logic
    color: enabled
        ? (Theme.isDark ? "#E6E1E5" : "#1C1B1F")
        : (Theme.isDark ? "#61E6E1E5" : "#611C1B1F")

    selectionColor: Theme.accentColor
    selectedTextColor: {
        var c = Qt.color(Theme.accentColor);
        return (c.hslLightness < 0.5) ? "#FFFFFF" : "#000000";
    }

    placeholderTextColor: enabled && activeFocus
        ? Theme.accentColor
        : (Theme.isDark ? "#CAC4D0" : "#49454F")

    verticalAlignment: TextInput.AlignVCenter

    Material.containerStyle: Material.Outlined

    cursorDelegate: CursorDelegate { }

    FloatingPlaceholderText {
        id: placeholder
        // 宽度计算已自动包含新的 leftPadding/rightPadding
        width: control.width - (control.leftPadding + control.rightPadding)
        text: control.placeholderText
        font: control.font
        color: control.placeholderTextColor
        elide: Text.ElideRight
        renderType: control.renderType

        filled: control.Material.containerStyle === Material.Filled
        verticalPadding: control.Material.textFieldVerticalPadding
        controlHasActiveFocus: control.activeFocus
        controlHasText: control.length > 0
        controlImplicitBackgroundHeight: control.implicitBackgroundHeight
        controlHeight: control.height
        leftPadding: control.leftPadding
        floatingLeftPadding: control.Material.textFieldHorizontalPadding
    }

    background: MaterialTextContainer {
        implicitWidth: 120
        implicitHeight: control.Material.textFieldHeight

        filled: control.Material.containerStyle === Material.Filled
        fillColor:  control.Material.containerStyle === Material.Filled ? (Theme.isDark ? "#49454F" : "#E7E0EC") : "transparent"

        outlineColor: (enabled && control.hovered)
            ? (Theme.isDark ? "#E6E1E5" : "#1C1B1F")
            : (enabled ? (Theme.isDark ? "#938F99" : "#79747E")
                : (Theme.isDark ? "#33938F99" : "#3379747E"))

        focusedOutlineColor: Theme.accentColor

        placeholderTextWidth: Math.min(placeholder.width, placeholder.implicitWidth) * placeholder.scale
        placeholderTextHAlign: control.effectiveHorizontalAlignment
        controlHasActiveFocus: control.activeFocus
        controlHasText: control.length > 0
        placeholderHasText: placeholder.text.length > 0
        horizontalPadding: control.Material.textFieldHorizontalPadding

        // 3. 左侧 Item 容器
        // 容器本身不设置大小，大小完全跟随 control.leftItem
        Item {
            id: leftItemContainer
            visible: control.leftItem != null
            anchors.left: parent.left
            anchors.leftMargin: control.Material.textFieldHorizontalPadding
            anchors.verticalCenter: parent.verticalCenter

            // 关键：容器尺寸跟随外部 Item
            width: control.leftItemWidth
            height: control.leftItem ? (control.leftItem.height || control.leftItem.implicitHeight || 0) : 0

            // 将外部传入的 leftItem 放入此处
            // 此时 leftItem 的 parent 变成了 leftItemContainer
            // 用户的 anchors.centerIn: parent 就会相对于这个容器生效
            data: [ control.leftItem ]
        }
    }

    // 4. 右侧 Item 容器
    Item {
        id: rightItemContainer
        visible: control.rightItem != null
        anchors.right: parent.right
        anchors.rightMargin: control.Material.textFieldHorizontalPadding
        anchors.verticalCenter: parent.verticalCenter

        width: control.rightItemWidth
        height: control.rightItem ? (control.rightItem.height || control.rightItem.implicitHeight || 0) : 0

        data: [ control.rightItem ]
    }
}
