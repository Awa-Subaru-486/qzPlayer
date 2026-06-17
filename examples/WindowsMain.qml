import QtQuick
import QtQuick.Dialogs
import QtQuick.Controls
import QtQuick.Layouts
import qz.player
import qz.theme
import qz.controls.md3

PlayerWindow {
    id: root

    FileDropArea {
        anchors.fill: parent
        z: 999

        onFilesDropped: function(filePaths) {
            var urls = filePaths.map(function(path) { return "file:///" + path.replace(/\\/g, "/") })
            console.log(urls)
            root.addToPlaylist(urls)
        }
    }

    // Component.onCompleted: {
    //     var testMedia = [
    //         "file:///D:/AwaMedia/test_video/chinese_numbers.mkv",
    //         "file:///D:/AwaMedia/test_video/音视频 - 【4K HDR】【上海】竖屏的简短尝试｜EOS R5 ｜EF 85 1.4 IS｜.mp4",
    //         "file:///D:/AwaMedia/test_video/字幕测试/音视频 - 凡人修仙传【紫灵告白完整版+专属BGM 沧海飞尘】.mp4",
    //         "file:///E:/home/med/服务器测试资源/解码器 yuv420p 帧 rgb24.mp4",
    //         "file:///D:/AwaMedia/test_video/8k/禅_8K_HDR_50P_Rec.2020_10Bit_YUV420_2.39.1_HEVC_1000nits_20210304_logo.Final.Version.mp4",
    //         "file:///D:/AwaMedia/test_video/text/av1_amf_test.mp4",
    //         "file:///D:/AwaMedia/test_video/text/h264_amf_test.mp4",
    //         "file:///D:/AwaMedia/test_video/text/hevc_amf_test.mp4",
    //         "file:///D:/AwaMedia/test_video/text/vp9_libvpx_test.webm",
    //         "file:///E:/home/med/电影/新驯龙高手/新驯龙高手.mkv",
    //     ]
    //     setPlaylist(testMedia)
    //     play()
    // }
}
