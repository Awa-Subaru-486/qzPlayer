import QtQuick
import QtQuick.Controls
import qz.controls.md3

// 圆角图片组件，支持圆角裁剪和缩放效果
Item {
    id: root

    // ==========================================
    // 公开接口
    // ==========================================

    // [接口] 图片源路径
    property alias source: img.source

    // [接口] 图片原始尺寸
    property alias sourceSize: img.sourceSize

    // [接口] 填充模式
    property int fillMode: Image.PreserveAspectCrop

    // [接口] 统一圆角半径
    property real radius: 0

    // [接口] 独立圆角半径
    property real topLeftRadius:     0
    property real topRightRadius:    0
    property real bottomLeftRadius:  0
    property real bottomRightRadius: 0

    // [接口] 缩放动画系数
    property real imgScale: 1.0

    // [接口] 是否启用缩放动画
    property bool enableScale: false

    // [接口] 加载动画颜色
    property color loaderColor: "#FFFFFF"

    // ==========================================
    // 内部逻辑
    // ==========================================

    property var effectiveRadii: radius > 0
        ? Qt.vector4d(radius, radius, radius, radius)
        : Qt.vector4d(topLeftRadius, topRightRadius, bottomLeftRadius, bottomRightRadius)

    implicitWidth: img.implicitWidth
    implicitHeight: img.implicitHeight

    Behavior on imgScale {
        enabled: root.enableScale
        NumberAnimation {
            duration: 150
            easing.type: Easing.OutQuad
        }
    }

    Image { id: img
        visible: false
        asynchronous: true
        cache: true
        fillMode: root.fillMode
    }

    Rectangle {
        anchors.fill: parent
        visible: img.status === Image.Loading
        color: "#000000"
        radius: 25
        topLeftRadius: root.topLeftRadius
        topRightRadius: root.topRightRadius
        bottomLeftRadius: root.bottomLeftRadius
        bottomRightRadius: root.bottomRightRadius

        Md3Loader {
            anchors.centerIn: parent
            running: img.status === Image.Loading
            color: root.loaderColor
            size: Math.min(parent.width, parent.height) * 0.3
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#20000000"
        radius: root.radius
        topLeftRadius: root.topLeftRadius
        topRightRadius: root.topRightRadius
        bottomLeftRadius: root.bottomLeftRadius
        bottomRightRadius: root.bottomRightRadius

        visible: img.status === Image.Error

        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("加载失败")
                color: "#FFFFFF"
                font.pixelSize: Math.min(root.width, root.height) * 0.08
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("点击重试")
                color: "#FFFFFF"
                font.pixelSize: Math.min(root.width, root.height) * 0.06
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                var currentSource = img.source
                img.source = ""
                img.source = currentSource
            }
        }
    }

    readonly property vector4d fillModeTransform: {
        var srcW = img.sourceSize.width;
        var srcH = img.sourceSize.height;
        var dstW = root.width;
        var dstH = root.height;
        var sx = 1.0, sy = 1.0, ox = 0.0, oy = 0.0;
        if (srcW > 0 && srcH > 0 && dstW > 0 && dstH > 0) {
            var srcRatio = srcW / srcH;
            var dstRatio = dstW / dstH;
            if (root.fillMode === Image.PreserveAspectFit) {
                if (dstRatio > srcRatio) {
                    sx = srcRatio / dstRatio; ox = (1.0 - sx) / 2.0;
                } else {
                    sy = dstRatio / srcRatio; oy = (1.0 - sy) / 2.0;
                }
            } else if (root.fillMode === Image.PreserveAspectCrop) {
                if (dstRatio > srcRatio) {
                    sy = dstRatio / srcRatio; oy = (1.0 - sy) / 2.0;
                } else {
                    sx = srcRatio / dstRatio; ox = (1.0 - sx) / 2.0;
                }
            }
        }
        return Qt.vector4d(sx, sy, ox, oy);
    }

    ShaderEffect { id: effect
        anchors.fill: parent
        layer.enabled: true
        layer.smooth: true
        visible: img.status === Image.Ready && status !== ShaderEffect.Error

        fragmentShader: "qrc:/qz/player/shaders/RoundedImage.frag.qsb"
        vertexShader: "qrc:/qz/player/shaders/RoundedImage.vert.qsb"

        property var source: img
        property var size: Qt.vector2d(width, height)
        property var radii: root.effectiveRadii
        property var u_transform: root.fillModeTransform
        property real u_imgScale: root.imgScale

        onStatusChanged: {
            if (status === ShaderEffect.Error) {
                console.warn("RoundedImage: 着色器加载失败 -", effect.log)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#20000000"
        radius: root.radius
        topLeftRadius: root.topLeftRadius
        topRightRadius: root.topRightRadius
        bottomLeftRadius: root.bottomLeftRadius
        bottomRightRadius: root.bottomRightRadius

        visible: img.status === Image.Ready && effect.status === ShaderEffect.Error

        Text {
            anchors.centerIn: parent
            text: qsTr("着色器加载失败")
            color: "#FFFFFF"
            font.pixelSize: Math.min(root.width, root.height) * 0.08
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }
    }
}
