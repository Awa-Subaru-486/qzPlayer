// Md3RadioButton.qml - Material Design 3 风格单选按钮组件
import QtQuick
import QtQuick.Templates as T
import QtQuick.Controls.Material
import QtQuick.Controls.Material.impl
import qz.theme

T.RadioButton {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding,
        implicitIndicatorHeight + topPadding + bottomPadding)

    spacing: 8
    padding: 8
    verticalPadding: padding + 6

    // MD3 Color Tokens
    readonly property color colorOutlineLight: "#79747E"
    readonly property color colorOnSurfaceLight: "#1C1B1F"

    readonly property color colorOutlineDark: "#938F99"
    readonly property color colorOnSurfaceDark: "#E6E1E5"

    // Helper to handle opacity
    function withAlpha(colorObj, alpha) {
        return Qt.rgba(colorObj.r, colorObj.g, colorObj.b, alpha)
    }

    // Custom Indicator Implementation (Replacing RadioIndicator for full control)
    indicator: Item {
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding) : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 20
        implicitHeight: 20

        // 1. Outer Ring
        Rectangle {
            width: 20
            height: 20
            radius: 10
            anchors.centerIn: parent

            color: "transparent"
            border.width: 2

            // Logic: Checked -> Accent, Unchecked -> Outline
            border.color: {
                if (!control.enabled) {
                    // Disabled state: 38% opacity of Outline
                    return Theme.isDark ? withAlpha(colorOutlineDark, 0.38) : withAlpha(colorOutlineLight, 0.38)
                }

                if (control.checked) {
                    return Theme.accentColor
                }

                return Theme.isDark ? colorOutlineDark : colorOutlineLight
            }

            Behavior on border.color {
                ColorAnimation { duration: 200 }
            }
        }

        // 2. Inner Dot (Visible only when checked)
        Rectangle {
            width: 10
            height: 10
            radius: 5
            anchors.centerIn: parent

            color: Theme.accentColor
            visible: control.checked
            scale: control.checked ? 1 : 0

            // Smooth scaling animation
            Behavior on scale {
                NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
            }
        }

        // 3. Ripple Effect
        // Using Material.impl Ripple, but manually setting the color
        Ripple {
            x: (parent.width - width) / 2
            y: (parent.height - height) / 2
            width: 28
            height: 28
            z: -1
            anchor: control
            pressed: control.pressed
            active: control.down || control.visualFocus || control.hovered

            // Ripple Color Logic:
            // Checked: Uses Accent (Primary) with low opacity
            // Unchecked: Uses OnSurface with low opacity
            color: {
                if (control.checked) {
                    // 12% opacity of Accent
                    return withAlpha(Theme.accentColor, 0.12)
                }

                // Unchecked Ripple
                // Light: 8% of OnSurface (#1C1B1F)
                // Dark: 8% of OnSurface (#E6E1E5)
                return Theme.isDark ? withAlpha(colorOnSurfaceDark, 0.08)
                    : withAlpha(colorOnSurfaceLight, 0.08)
            }
        }
    }

    contentItem: Text {
        leftPadding: control.indicator && !control.mirrored ? control.indicator.width + control.spacing : 0
        rightPadding: control.indicator && control.mirrored ? control.indicator.width + control.spacing : 0

        text: control.text
        font: control.font

        // Text Color Logic
        color: {
            if (!control.enabled) {
                // Disabled text: 38% opacity of OnSurface
                return Theme.isDark ? withAlpha(colorOnSurfaceDark, 0.38)
                    : withAlpha(colorOnSurfaceLight, 0.38)
            }
            return Theme.isDark ? colorOnSurfaceDark : colorOnSurfaceLight
        }

        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter

        // Behavior for smooth color transition
        Behavior on color {
            ColorAnimation { duration: 200 }
        }
    }
}
