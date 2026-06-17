// Md3Loader.qml - Material Design 3 风格加载动画组件
import QtQuick
import qz.theme

Item {
    id: control

    property bool running: true
    property color color: Theme.accentColor
    property real size: 48

    implicitWidth: size
    implicitHeight: size

    ShaderEffect {
        id: blobEffect
        anchors.fill: parent
        anchors.margins: control.withContainer ? parent.width * 0.25 : parent.width * 0.1
        visible: control.running

        // 1. 动画状态属性（作为 Uniform 变量传给 GPU）
        property real morphProgress: 0

        // 2. 形状配置
        property var lobeSequence: [10, 9, 5, 2, 8, 4, 2]
        readonly property int sequenceLength: 7

        // 3. 计算当前的两个形状参数，传给 Shader
        // 这些计算只在状态改变时在 CPU 上运行一次，非常轻量
        property real lobes1: lobeSequence[Math.floor(morphProgress) % sequenceLength]
        property real lobes2: lobeSequence[(Math.floor(morphProgress) + 1) % sequenceLength]
        property real mixFactor: morphProgress - Math.floor(morphProgress)

        // 4. 传递颜色和尺寸
        property color uColor: control.color
        property real uAspectRatio: width / height

        ParallelAnimation {
            running: control.running && control.visible
            loops: Animation.Infinite

            NumberAnimation {
                target: control
                property: "rotation"
                from: 0
                to: 360
                duration: 3000
                easing.type: Easing.Linear
                loops: Animation.Infinite
            }

            // 形状变形循环
            SequentialAnimation {
                loops: Animation.Infinite
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 0; to: 1; duration: 600; easing.type: Easing.InOutQuart }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 1; to: 2; duration: 600; easing.type: Easing.InOutQuart }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 2; to: 3; duration: 600; easing.type: Easing.InOutQuart }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 3; to: 4; duration: 600; easing.type: Easing.InOutQuart }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 4; to: 5; duration: 600; easing.type: Easing.InOutQuart }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 5; to: 6; duration: 600; easing.type: Easing.InOutQuart }
                PauseAnimation { duration: 800 }
                NumberAnimation { target: blobEffect; property: "morphProgress"; from: 6; to: 7; duration: 600; easing.type: Easing.InOutQuart }
                ScriptAction { script: blobEffect.morphProgress = 0 }
            }
        }

        fragmentShader: "qrc:/qz/controls/md3/src/shaders/Loader.frag.qsb"
    }
}
