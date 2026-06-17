// Md3Switch.qml - Material Design 3 风格开关组件
import QtQuick
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import QtQuick.Templates as T
import qz.theme

T.Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding,
        implicitIndicatorHeight + topPadding + bottomPadding)

    padding: 8
    spacing: 8

    icon.width: 16
    icon.height: 16
    icon.color: checked
        ? (!Theme.isDark
            ? enabled ? Qt.darker(Theme.accentColor, 1.8) : "#9E9E9E"
            : enabled ? "#FFFFFF" : "#616161")
        : enabled ? (Theme.isDark ? "#938F99" : "#79747E") : (Theme.isDark ? "#49454F" : "#BDBDBD")

    indicator: Md3SwitchIndicator {
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        control: control

        Ripple {
            x: parent.handle.x + parent.handle.width / 2 - width / 2
            y: parent.handle.y + parent.handle.height / 2 - height / 2
            width: 28
            height: 28
            pressed: control.pressed
            active: enabled && (control.down || control.visualFocus || control.hovered)
            color: control.checked ? Qt.alpha(Theme.accentColor, 0.12) : (Theme.isDark ? Qt.alpha("#938F99", 0.12) : Qt.alpha("#79747E", 0.12))
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font: control.font
        color: control.enabled ? (Theme.isDark ? "#E6E1E5" : "#1D1B20") : (Theme.isDark ? "#49454F" : "#BDBDBD")
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }
}
