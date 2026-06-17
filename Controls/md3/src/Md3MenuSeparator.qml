// Md3MenuSeparator.qml - Material Design 3 风格菜单分割线组件
import QtQuick
import QtQuick.Templates as T
import qz.theme

T.MenuSeparator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
        implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
        implicitContentHeight + topPadding + bottomPadding)

    verticalPadding: 8

    contentItem: Rectangle {
        implicitWidth: 200
        implicitHeight: 1

        // MD3 颜色标准：分割线
        // 浅色模式：#1F000000 (约 12% 不透明度的黑色)
        // 深色模式：#1FFFFFFF (约 12% 不透明度的白色)
        color: Theme.isDark ? "#1FFFFFFF" : "#1F000000"
    }
}
