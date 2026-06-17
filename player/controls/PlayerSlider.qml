import QtQml
import QtQuick
import QtQuick.Controls
import qz.theme

// 播放器进度滑块，支持预览帧、章节标记和拖拽定位
Item { id: root
    property bool mediaPlay: true
    property bool entered: mouseArea.containsMouse
    property bool active: false
    property int canvasSize: Qt.platform.os === "android" ? 6 : 4
    property double mousePosition: 0
    property double from: 0.0
    property double to: 100.0
    property double cacheValue: 0.0
    property var previewFrames: []
    property url mediaSource: ""
    property var chapters: []
    property double opening: 0.0
    property double ending: 0.0
    property color backCanvasColor: Theme.isDark ? "#999087" : "#cccccc"
    property color frontCanvasColor: Theme.accentColor

    signal seek(double pos)

    property double effectiveTo: Math.max(to, from + 1)

    Behavior on width {
        NumberAnimation {
            duration: 250
            easing.type: Easing.Linear
        }
    }

    QtObject { id: sliderData
        property double value: 0.0
        property double mouseAreaValue: 0.0

        onMouseAreaValueChanged: {
            if(mouseAreaValue <= root.effectiveTo && mouseAreaValue >= root.from) {
                root.seek(mouseAreaValue.toFixed(2))
            }
        }
    }

    onMediaPlayChanged: {
        mediaStateCanvas.requestPaint()
    }

    function setValue(value) {
        if(!mouseArea.drag.active) {
            sliderData.value = value
        }
    }

    function correctionValue(value) {
        if (value >= 0.5) {
            return Math.floor(value) + 1;
        } else {
            return Math.floor(value);
        }
    }

    function clamp(value, min, max) {
        return Math.max(min, Math.min(value, max));
    }

    function mapValueRange(value, srcStart, srcEnd, dstStart, dstEnd) {
        if (srcStart === srcEnd)
            return dstStart;

        var minVal = Math.min(srcStart, srcEnd);
        var maxVal = Math.max(srcStart, srcEnd);
        var clampedValue = Math.max(minVal, Math.min(value, maxVal));

        return dstStart + (clampedValue - srcStart) * (dstEnd - dstStart) / (srcEnd - srcStart);
    }

    function formatTime(seconds_value) {
        var time = Math.round(seconds_value);
        var hours = Math.floor(time / 3600);
        var minutes = Math.floor((time % 3600) / 60);
        var seconds = Math.floor(time % 60);

        var pad = function(num) { return num.toString().padStart(2, '0'); };

        if (hours > 0) {
            return pad(hours) + ":" + pad(minutes) + ":" + pad(seconds);
        } else {
            return pad(minutes) + ":" + pad(seconds);
        }
    }

    function updatePromptPositions(item, x) {
        item.x = clamp(x - item.width / 2.0, -(item.width/2.0), to_canvas.width - item.width / 2.0) + to_canvas.x
    }

    Behavior on canvasSize {
        NumberAnimation {
            duration: 250
            easing.type: Easing.Linear
        }
    }

    Item { id: handleItem
        anchors.right: value_canvas.right
        anchors.verticalCenter: value_canvas.verticalCenter
    }

    Item { id: dragRect
    }

    MouseArea { id: mouseArea
        anchors.centerIn: parent
        width: to_canvas.width
        height: handle.height
        hoverEnabled: true
        drag.target: dragRect

        onReleased: {
            sliderData.value = mapValueRange(correctionValue(mouseX),
                0, to_canvas.width, root.from, root.effectiveTo)
            sliderData.mouseAreaValue = mapValueRange(correctionValue( mouseX),
                0, to_canvas.width, root.from, root.effectiveTo)
        }

        onPositionChanged: {
            root.active = true
            updatePromptPositions(seekPointer, mouseX)

            root.mousePosition = mapValueRange(correctionValue(mouseX),
                0,  to_canvas.width, root.from, root.effectiveTo)

            if(drag.active) {
                sliderData.value = mapValueRange(correctionValue(mouseX),
                    0, to_canvas.width, root.from, root.effectiveTo)
            }
        }

        onEntered: {
            root.canvasSize = Qt.platform.os === "android" ? 8 : 6
            root.active = true
        }

        onExited: {
            root.canvasSize = Qt.platform.os === "android" ? 6 : 4
            root.active = false
        }
    }

    Item { id: seekPointer
        implicitWidth: pointer.width
        height: root.canvasSize + pointer.height * 2
        anchors.verticalCenter: parent.verticalCenter
        visible: Qt.platform.os !== "android"
        scale: root.entered ? 1.0 : 0.0
        Behavior on scale {
            NumberAnimation {
                duration: 200
                easing.type: Easing.Linear
            }
        }

        Canvas { id: pointer
            implicitWidth: 8
            implicitHeight: 4
            anchors.top: parent.top
            scale: root.entered ? 1.0 : 0.0
            onPaint: {
                var ctx = getContext('2d');
                ctx.fillStyle = frontCanvasColor;
                ctx.beginPath();
                ctx.moveTo(0, 0);
                ctx.lineTo( width, 0 );
                ctx.lineTo(width/2.0, height);
                ctx.closePath();
                ctx.fill();
            }
        }

        Canvas {
            implicitWidth: 8
            implicitHeight: 4
            anchors.bottom: parent.bottom
            onPaint: {
                var ctx = getContext('2d');
                ctx.fillStyle = frontCanvasColor;
                ctx.beginPath();
                ctx.moveTo(0, height);
                ctx.lineTo( width / 2.0, 0);
                ctx.lineTo(width, height);
                ctx.closePath();
                ctx.fill();
            }
        }

        SeekFrame { id: seek_frame
            anchors.bottom: parent.top
            anchors.bottomMargin: 15
            time: formatTime(mousePosition)
            previewFrames: root.previewFrames
            mediaSource: root.mediaSource
            currentTime: Math.round(mousePosition * 1000)
            chapters: root.chapters
        }
    }

    Rectangle { id: to_canvas
        color: Qt.rgba(backCanvasColor.r, backCanvasColor.g, backCanvasColor.b, 0.33)
        anchors.centerIn: parent
        width:  parent.width - handle.width
        height: canvasSize
        radius: 10
    }

    Repeater { id: chapter_markers
        model: root.chapters
        delegate: Rectangle {
            required property var modelData
            readonly property double chapterPos: root.effectiveTo > root.from
                ? (modelData.startTime / 1000 - root.from) / (root.effectiveTo - root.from) * to_canvas.width
                : 0
            x: to_canvas.x + chapterPos - 3
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Qt.rgba(frontCanvasColor.r, frontCanvasColor.g, frontCanvasColor.b, 0.7)
            visible: root.chapters.length > 0 && chapterPos > 0 && chapterPos < to_canvas.width
        }
    }

    // 片头区域高亮
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: to_canvas.x
        width: root.opening > 0 && root.effectiveTo > root.from
            ? clamp(root.opening / (root.effectiveTo - root.from) * to_canvas.width, 0, to_canvas.width)
            : 0
        height: canvasSize
        radius: 10
        color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
        visible: root.opening > 0
    }

    // 片尾区域高亮
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: root.ending > 0 && root.effectiveTo > root.from
            ? to_canvas.x + clamp((root.effectiveTo - root.ending) / (root.effectiveTo - root.from) * to_canvas.width, 0, to_canvas.width)
            : to_canvas.x
        width: root.ending > 0 && root.effectiveTo > root.from
            ? clamp(root.ending / (root.effectiveTo - root.from) * to_canvas.width, 0, to_canvas.width)
            : 0
        height: canvasSize
        radius: 10
        color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(0, 0, 0, 0.06)
        visible: root.ending > 0
    }

    // 片头分割标记（圆角倒立正方形/菱形）
    Canvas { id: openingMarker
        readonly property double markerX: root.opening > 0 && root.effectiveTo > root.from
            ? to_canvas.x + (root.opening / (root.effectiveTo - root.from)) * to_canvas.width
            : -width
        x: markerX - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 8
        height: 8
        visible: root.opening > 0 && markerX > 0 && markerX < to_canvas.width

        onPaint: {
            var ctx = getContext('2d');
            ctx.clearRect(0, 0, width, height);
            ctx.fillStyle = Qt.rgba(frontCanvasColor.r, frontCanvasColor.g, frontCanvasColor.b, 0.8);
            ctx.beginPath();
            var r = 1.5;
            ctx.moveTo(width / 2, r);
            ctx.lineTo(width - r, height / 2);
            ctx.lineTo(width / 2, height - r);
            ctx.lineTo(r, height / 2);
            ctx.closePath();
            ctx.fill();
        }

        onVisibleChanged: requestPaint()
    }

    // 片尾分割标记（圆角倒立正方形/菱形）
    Canvas { id: endingMarker
        readonly property double markerX: root.ending > 0 && root.effectiveTo > root.from
            ? to_canvas.x + ((root.effectiveTo - root.ending) / (root.effectiveTo - root.from)) * to_canvas.width
            : -width
        x: markerX - width / 2
        anchors.verticalCenter: parent.verticalCenter
        width: 8
        height: 8
        visible: root.ending > 0 && markerX > 0 && markerX < to_canvas.width

        onPaint: {
            var ctx = getContext('2d');
            ctx.clearRect(0, 0, width, height);
            ctx.fillStyle = Qt.rgba(frontCanvasColor.r, frontCanvasColor.g, frontCanvasColor.b, 0.8);
            ctx.beginPath();
            var r = 1.5;
            ctx.moveTo(width / 2, r);
            ctx.lineTo(width - r, height / 2);
            ctx.lineTo(width / 2, height - r);
            ctx.lineTo(r, height / 2);
            ctx.closePath();
            ctx.fill();
        }

        onVisibleChanged: requestPaint()
    }

    Rectangle { id: cache_value_canvas
        color: Theme.isDark ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(0, 0, 0, 0.12)
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: to_canvas.left
        width: clamp(
            mapValueRange(root.cacheValue, root.from, root.effectiveTo, 0, to_canvas.width),
            0, to_canvas.width
        )
        height: canvasSize
        radius: 10
    }

    Rectangle {
        id: value_canvas
        color: frontCanvasColor
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: to_canvas.left
        width: clamp(mapValueRange(
                sliderData.value, root.from, root.effectiveTo, 0, to_canvas.width),
            0, to_canvas.width
        )

        height: canvasSize
        radius: 10
    }

    Rectangle { id: handle
        color: "#FFFFFF"
        border.color: "#6e6c6c"
        border.width: 1.5
        width: 19
        height: 15
        radius: 3
        scale: root.entered ? 1.0 : 0.0
        anchors.centerIn: handleItem

        Behavior on scale {
            NumberAnimation {
                duration: 350
                easing.type: Easing.Linear
            }
        }

        Canvas {
            anchors.bottom: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            height: 4

            onPaint: {
                var ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                ctx.beginPath()
                ctx.moveTo(width*0.25, 0)
                ctx.lineTo(width*0.4, height)
                ctx.moveTo(width*0.75, 0)
                ctx.lineTo(width*0.6, height)

                ctx.strokeStyle = "#6e6c6c"
                ctx.lineWidth = 1
                ctx.stroke()
            }
        }

        Canvas { id: mediaStateCanvas
            anchors.fill: parent

            onPaint: {
                var ctx = getContext("2d")
                var radius = 3
                ctx.fillStyle = "#6e6c6c"
                ctx.clearRect(0,0, width, height)
                ctx.beginPath()
                if(root.mediaPlay) {
                    ctx.roundedRect(width / 2 - 5, height / 4, 3, height / 2, radius, radius)
                    ctx.roundedRect(width / 2 + 2, height / 4, 3, height / 2, radius, radius)
                } else {
                    ctx.moveTo(width / 2 - 2, height / 2 - 3)
                    ctx.lineTo(width / 2 - 2, height / 2 + 3)
                    ctx.lineTo(width / 2 + 4, height / 2)
                }
                ctx.fill();
            }
        }
    }
}
