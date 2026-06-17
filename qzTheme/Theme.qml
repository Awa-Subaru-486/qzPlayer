pragma Singleton

import QtQuick
import qz.theme

QtObject {
    id: root

    property ThemeConfig config: ThemeConfig {}

    readonly property bool isDark: config.activeTheme === ThemeType.Dark

    readonly property font font: config.font
    readonly property color accentColor: config.accentColor
    readonly property color textColor: isDark ? "#E9E9E9" : "#000000"

    property int roundedScale: 28
    readonly property string backgroundImage: ""

    property int activeTheme: config.activeTheme

    function setTheme(themeType) {
        root.config.activeTheme = themeType
    }

    function overrideAccentColor(color) {
        root.config.overrideAccentColor(color)
    }

    function restoreAccentColor() {
        root.config.restoreAccentColor()
    }
}
