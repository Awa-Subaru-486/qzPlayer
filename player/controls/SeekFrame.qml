import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQml
import qz.theme
import qz.multimedia

// 进度条悬停预览框，显示时间、章节和视频预览帧
Rectangle { id: root
    property int margin: 4
    property string time: "00:00"
    property var previewFrames: []
    property int currentTime: 0
    property var chapters: []
    property string previewSource: ""
    property url mediaSource: ""
    property real previewWidth: 160
    property real previewHeight: 90

    border.color: Theme.isDark ? "#6e6c6c" : "#cccccc"
    border.width: 1
    radius: 8
    color: Theme.isDark ? "#aa000000" : "#aaffffff"

    readonly property bool hasPreview: false

    readonly property string currentChapterTitle: {
        if (chapters.length === 0) return ""
        for (var i = chapters.length - 1; i >= 0; --i) {
            if (currentTime >= chapters[i].startTime) return chapters[i].title
        }
        return ""
    }

    readonly property bool hasChapter: currentChapterTitle !== ""

    readonly property real frameAspect: previewWidth / previewHeight

    readonly property real frameWidth: previewWidth
    readonly property real frameHeight: previewWidth / frameAspect

    implicitWidth: hasPreview ? frameWidth : timeRect.width
    implicitHeight: hasPreview ? frameHeight + timeRect.height + 10 : timeRect.height

    function updatePosition() {
        x = 0;
        var pointInRoot = mapToItem(parent, 0, 0)
        x = pointInRoot.x + ((parent.width - root.width) / 2)

        var rootWindowPos = mapToItem(null, 0, 0)
        if(rootWindowPos.x < margin) {
            x = -(parent.mapToItem(null, 0, 0).x) + margin;
            return
        }

        var windowItemW = Window.window.width - margin;
        var rootWindowPosMaxX = rootWindowPos.x + width;
        if(rootWindowPosMaxX > windowItemW) {
            x = windowItemW - parent.mapToItem(null, 0, 0).x - width;
        }
    }

    Connections {
        target: parent
        function onXChanged() {
            updatePosition()
        }
    }

    // PreviewFrame { id: previewImage
    //     anchors.top: parent.top
    //     anchors.left: parent.left
    //     anchors.right: parent.right
    //     height: hasPreview ? frameHeight : 0
    //     source: root.mediaSource
    //     position: root.currentTime
    //     radius: 6
    //     visible: hasPreview
    // }

    Rectangle { id: timeRect
        radius: 8
        color: hasPreview ? Qt.rgba(0, 0, 0, 0.66) : "transparent"
        width: hasChapter ? Math.max(timeText.implicitWidth, chapterText.implicitWidth) + 20 : timeText.implicitWidth + 20
        height: hasChapter ? timeText.height + chapterText.height + 18 : timeText.height + 14
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: hasPreview ? 10 : 0
        Column {
            anchors.centerIn: parent
            spacing: 2
            Text { id: timeText
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 14
                font.bold: true
                color: "#ffffff"
                text: root.time
            }
            Text { id: chapterText
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: 11
                elide: Text.ElideRight
                color: "#cccccc"
                text: root.currentChapterTitle
                visible: root.hasChapter
            }
        }
    }
}
