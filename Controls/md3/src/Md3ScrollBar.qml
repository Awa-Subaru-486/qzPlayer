import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import qz.theme

// Material Design 3 风格滚动条
T.ScrollBar { id: control
    policy: ScrollBar.AsNeeded

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    minimumSize: orientation === Qt.Horizontal ? height / width : width / height

    background: Item {}

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: width / 2

        color: {
            var baseColor = Theme.isDark ? "#474747" : "#D6D7D7"

            if (control.pressed || control.hovered) {
                return Qt.alpha(Theme.accentColor, 0.8)
            }

            return Qt.tint(baseColor, "#30FFFFFF")
        }

        Behavior on color {
            ColorAnimation { duration: 200 }
        }

        opacity: 0.0
    }

    states: State {
        name: "active"
        when: control.policy === T.ScrollBar.AlwaysOn || (control.active && control.size < 1.0)
        PropertyChanges { target: control.contentItem; opacity: 1.0 }
    }

    transitions: [
        Transition {
            to: "active"
            NumberAnimation { target: control.contentItem; property: "opacity"; to: 1.0; duration: 200 }
        },
        Transition {
            from: "active"
            SequentialAnimation {
                PropertyAction { target: control.contentItem; property: "opacity"; value: 1.0 }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: control.contentItem; property: "opacity"; to: 0.0; duration: 200 }
            }
        }
    ]
}
