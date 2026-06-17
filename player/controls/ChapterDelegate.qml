import QtQuick
import qz.theme

// 章节项委托
Rectangle {
    id: root

    required property var modelData
    required property int index
    required property var mediaPlayer
    required property bool showHoverEffect
    required property bool isCurrent

    property int delegateHeight: 36

    width: parent ? parent.width : 0
    height: delegateHeight
    radius: 6
    color: {
        if (isCurrent) return Theme.accentColor
        return "transparent"
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        text: modelData.title
        font.pixelSize: 13
        color: Theme.textColor
        elide: Text.ElideRight
    }

    MouseArea {
        id: chapterDelegateMouseArea
        anchors.fill: parent
        hoverEnabled: showHoverEffect
        cursorShape: Qt.PointingHandCursor
        onClicked: mediaPlayer.position = modelData.startTime
    }
}
