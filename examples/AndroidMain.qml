import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import qz.player
import qz.theme
import qz.controls.md3

PlayerWindow {
    id: root

    property string currentUrl: ""

    // Bottom control button for Android
    Md3Button {
        id: controlButton
        z: 20
        text: qsTr("控制面板")
        flat: true

        anchors {
            horizontalCenter: parent.horizontalCenter
            bottom: parent.bottom
            bottomMargin: 80
        }

        visible: root.isShowControls
        onClicked: controlDialog.open()
    }

    // Control panel dialog
    Dialog {
        id: controlDialog
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: Math.min(root.width * 0.85, 400)
        modal: true
        title: qsTr("打开媒体")
        padding: 16

        background: Rectangle {
            radius: 12
            color: Theme.isDark ? "#dd1a1a1a" : "#ddf0f0f0"
            border.color: Theme.isDark ? "#33ffffff" : "#22000000"
            border.width: 1
        }

        contentItem: ScrollView {
            clip: true
            implicitHeight: Math.min(root.height * 0.6, 300)

            Column {
                spacing: 10
                width: controlDialog.availableWidth

                Row {
                    spacing: 8

                    Md3Button {
                        text: qsTr("打开本地视频")
                        flat: true
                        onClicked: fileDialog.open()
                    }

                    Md3Button {
                        text: qsTr("打开URL")
                        flat: true
                        onClicked: {
                            if (urlTextField.text !== "") {
                                root.addToPlaylist([urlTextField.text])
                                root.play()
                                controlDialog.close()
                            }
                        }
                    }
                }

                Text {
                    text: qsTr("当前地址")
                    font.pixelSize: 12
                    color: Theme.isDark ? "#aaffffff" : "#aa000000"
                }

                TextField {
                    id: urlTextField
                    width: parent.width
                    placeholderText: qsTr("输入或查看URL")
                    color: Theme.textColor
                    font.pixelSize: 12
                    selectByMouse: true

                    background: Rectangle {
                        radius: 6
                        color: Theme.isDark ? "#2a2a2a" : "#ffffff"
                        border.color: urlTextField.activeFocus ? Theme.accentColor : (Theme.isDark ? "#444444" : "#cccccc")
                        border.width: 1
                    }

                    onAccepted: {
                        if (text !== "") {
                            root.addToPlaylist([text])
                            root.play()
                            controlDialog.close()
                        }
                    }
                }
            }
        }
    }

    // File dialog for opening local videos
    FileDialog {
        id: fileDialog
        title: qsTr("选择视频文件")
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Video files (*.mp4 *.mkv *.avi *.webm *.mov *.flv *.ts *.m3u8)", "All files (*)"]
        onAccepted: {
            var urls = []
            for (var i = 0; i < selectedFiles.length; i++) {
                var url = selectedFiles[i]
                console.log("[AndroidMain] FileDialog selected file:", url.toString())
                urls.push(url)
            }
            if (urls.length > 0) {
                console.log("[AndroidMain] Adding", urls.length, "file(s) to playlist")
                root.addToPlaylist(urls)
                controlDialog.close()
            } else {
                console.log("[AndroidMain] FileDialog returned no files")
            }
        }
    }
}
