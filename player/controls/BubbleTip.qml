import QtQuick
import qz.theme

// 气泡提示弹出框，带箭头指示和显示/隐藏动画
Item {
    id: root
    default property alias contentData: bubble.data
    property color bubbleColor: Theme.isDark ? "#dd1a1a1a" : "#ddf0f0f0"
    property int spacing: 4
    property bool updateWidth: false
    property bool updateHeight: false
    property bool _showing: false
    property bool _hiding: false
    readonly property real margin: 4
    readonly property alias containsMouse: hoverHandler.hovered

    property real _targetY: 0

    opacity: 0.0
    visible: false

    implicitWidth: 40
    implicitHeight: 35

    function show() {
        if (_showing) return
        _showing = true
        _hiding = false
        hideAnim.stop()
        updatePosition()
        y = _targetY + 10
        visible = true
        showAnim.restart()
    }

    function hide() {
        if (_hiding) return
        _hiding = true
        _showing = false
        showAnim.stop()
        hideAnim.start()
    }

    onWidthChanged: {
        if (!updateWidth) {
            updateWidth = true
            width += arrow.width
        }
    }

    onHeightChanged: {
        if (!updateHeight) {
            updateHeight = true
            height += arrow.height
        }
    }

    function updatePosition() {
        x = 0
        y = 0
        arrow.x = (bubble.width - arrow.width) / 2.0
        var pointInRoot = mapToItem(parent, 0, 0)
        x = pointInRoot.x + ((parent.width - root.width) / 2)
        _targetY = pointInRoot.y - root.height - spacing

        var rootWindowPos = mapToItem(null, 0, 0)
        if (rootWindowPos.x < margin) {
            var newX = -(parent.mapToItem(null, 0, 0).x) + margin
            var oldX = x
            x = newX
            arrow.x += oldX - newX
            return
        }
        var windowItemW = Window.window ? Window.window.width - margin : 9999
        var rootWindowPosMaxX = rootWindowPos.x + width
        if (rootWindowPosMaxX > windowItemW) {
            var newX = windowItemW - parent.mapToItem(null, 0, 0).x - width
            var oldX = x
            arrow.x += oldX - newX
            x = newX
        }
    }

    HoverHandler { id: hoverHandler }

    ParallelAnimation {
        id: showAnim
        running: false
        NumberAnimation {
            target: root
            property: "opacity"
            to: 1.0
            duration: 150
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            target: root
            property: "y"
            to: root._targetY
            duration: 150
            easing.type: Easing.OutCubic
        }
    }

    SequentialAnimation {
        id: hideAnim
        running: false
        PauseAnimation { duration: 100 }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "opacity"
                to: 0.0
                duration: 100
                easing.type: Easing.InCubic
            }
            NumberAnimation {
                target: root
                property: "y"
                to: root._targetY + 10
                duration: 100
                easing.type: Easing.InCubic
            }
        }
        ScriptAction { script: root.visible = false }
    }

    Canvas {
        id: arrow
        width: 14
        height: 8
        contextType: "2d"
        antialiasing: true
        anchors.bottom: parent.bottom

        onPaint: {
            var ctx = getContext("2d")
            ctx.fillStyle = bubble.color
            ctx.clearRect(0, 0, width, height)
            ctx.beginPath()
            ctx.moveTo(0, 0)
            ctx.lineTo(width / 2.0, height)
            ctx.lineTo(width, 0)
            ctx.closePath()
            ctx.fill()
        }

        Component.onCompleted: requestPaint()
    }

    Rectangle {
        id: bubble
        color: root.bubbleColor
        radius: 8
        clip: true

        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: arrow.top
        }
    }
}
