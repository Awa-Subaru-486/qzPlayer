# qzPlayer

基于 Qt 6 / C++23 / FFmpeg 的跨平台多媒体播放器，支持 Windows 和 Android。

## 项目结构

```
├── multimedia/          # qzMultimedia - 多媒体核心库 (FFmpeg 插件、音频、视频)
├── player/              # qzPlayer - 播放器 UI 库 (QML 模块)
├── Controls/md3/        # Md3Controls - Material Design 3 控件库
├── qzTheme/             # qzTheme - 主题系统库
├── qzLog/               # qzLog - 日志库 (spdlog + fmt, C++20 module)
├── examples/            # 示例应用 (qzPlayerExample)
├── cmake/               # CMake 工具函数
└── CMakeUserPresets.json # 构建预设配置
```

## UI 预览

### 桌面端

![Desktop Preview 1](https://private-user-images.githubusercontent.com/248678592/609181380-c5938ee2-ead4-4a19-b319-014790801c0d.png?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3ODE2OTgxNzUsIm5iZiI6MTc4MTY5Nzg3NSwicGF0aCI6Ii8yNDg2Nzg1OTIvNjA5MTgxMzgwLWM1OTM4ZWUyLWVhZDQtNGExOS1iMzE5LTAxNDc5MDgwMWMwZC5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjYwNjE3JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI2MDYxN1QxMjA0MzVaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT05MTI4ZGY3MjQxYWE0YTE5YjU1YjZkNzVkYmM4OGY1ZWIxMTNkYmMyMWNhNTA0YmI1NWFjZmJlMjQxY2JkYTA1JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZyZXNwb25zZS1jb250ZW50LXR5cGU9aW1hZ2UlMkZwbmcifQ.r0jvlBwx-ica0w8eQJSs34Ft-F9D9UMLkwY8wdwKo_c)

![Desktop Preview 2](https://private-user-images.githubusercontent.com/248678592/609181358-449896a6-6bb8-453b-942d-1747e2a1fe3d.png?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3ODE2OTgxNzUsIm5iZiI6MTc4MTY5Nzg3NSwicGF0aCI6Ii8yNDg2Nzg1OTIvNjA5MTgxMzU4LTQ0OTg5NmE2LTZiYjgtNDUzYi05NDJkLTE3NDdlMmExZmUzZC5wbmc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjYwNjE3JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI2MDYxN1QxMjA0MzVaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT02YjA5OGNhMzI1NjQ0NGIxOTZiY2U2ZDI1MTQzNDRhNjQ5NjA2MzBhNWJkMzNhNTI5NDNlNTQxZWQ1MzZkMzUzJlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZyZXNwb25zZS1jb250ZW50LXR5cGU9aW1hZ2UlMkZwbmcifQ.eganxqhBH427d2KInQ1AmzuhQlYVubmPF9JvTt2sEqg)

### 移动端

![Mobile Preview 1](https://private-user-images.githubusercontent.com/248678592/609181162-9b28c5b7-9712-400d-b1b3-55e1ae5e3df7.jpg?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3ODE2OTgxNzUsIm5iZiI6MTc4MTY5Nzg3NSwicGF0aCI6Ii8yNDg2Nzg1OTIvNjA5MTgxMTYyLTliMjhjNWI3LTk3MTItNDAwZC1iMWIzLTU1ZTFhZTVlM2RmNy5qcGc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjYwNjE3JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI2MDYxN1QxMjA0MzVaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT0xNjBmOWUxZmE5ODAzNjU5NTA3N2MxMGMyNjQyMzEyYTQ2MDBmMzI4NTg2ODVkNTAxZDE4NjVmNjE1Zjk1MGEwJlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZyZXNwb25zZS1jb250ZW50LXR5cGU9aW1hZ2UlMkZqcGVnIn0.2uwR24D7eOlWn9RNcB1cC0oqGfZTJ-l_r86vRR7j7bo)

![Mobile Preview 2](https://private-user-images.githubusercontent.com/248678592/609181308-80dd585b-fdc2-4936-864e-c0b5de0802ad.jpg?jwt=eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9.eyJpc3MiOiJnaXRodWIuY29tIiwiYXVkIjoicmF3LmdpdGh1YnVzZXJjb250ZW50LmNvbSIsImtleSI6ImtleTUiLCJleHAiOjE3ODE2OTgxNzUsIm5iZiI6MTc4MTY5Nzg3NSwicGF0aCI6Ii8yNDg2Nzg1OTIvNjA5MTgxMzA4LTgwZGQ1ODViLWZkYzItNDkzNi04NjRlLWMwYjVkZTA4MDJhZC5qcGc_WC1BbXotQWxnb3JpdGhtPUFXUzQtSE1BQy1TSEEyNTYmWC1BbXotQ3JlZGVudGlhbD1BS0lBVkNPRFlMU0E1M1BRSzRaQSUyRjIwMjYwNjE3JTJGdXMtZWFzdC0xJTJGczMlMkZhd3M0X3JlcXVlc3QmWC1BbXotRGF0ZT0yMDI2MDYxN1QxMjA0MzVaJlgtQW16LUV4cGlyZXM9MzAwJlgtQW16LVNpZ25hdHVyZT02ZjA5ZDYxOGYzZDhiZWRmZmQxNGEyNTQ2MDNmOTJhNDRlOTM5ODNjNWJkMjBiNzliMzMxOWIwMDhiZTBhZGY1JlgtQW16LVNpZ25lZEhlYWRlcnM9aG9zdCZyZXNwb25zZS1jb250ZW50LXR5cGU9aW1hZ2UlMkZqcGVnIn0._6VLp8N0QoLqKISTBtjpiSkyihnRVFWA2bBw7iKl6xk)

## 技术栈

| 项目     | 版本 / 说明                                                |
| ------ | ------------------------------------------------------ |
| CMake  | 4.2.1+                                                 |
| C++    | C++23, 含 C++20 Modules (.cppm)                         |
| Qt     | 6.11.1 (Quick, Core, Gui, Network, Concurrent)         |
| FFmpeg | 8.0.2 (avformat, avcodec, avutil, swresample, swscale) |
| 构建工具   | Ninja + LLVM-MinGW (Windows), NDK 28.1 (Android)       |
| Vulkan | 1.4 (可选, 用于硬件加速视频解码)                                   |
| 日志     | spdlog 1.17.0 + fmt 12.1.0                             |

## 平台支持

| 平台      | ABI       | 音频后端               | 视频硬件加速          |
| ------- | --------- | ------------------ | --------------- |
| Windows | x86\_64   | WASAPI + PortAudio | D3D11VA, Vulkan |
| Android | arm64-v8a | AAudio             | Vulkan          |
| Android | x86\_64   | AAudio             | Vulkan          |

## 构建

### 前置条件

- **Qt 6.11.1** - 安装对应平台的 Qt 模块
- **LLVM-MinGW** - Windows 编译器
- **Vulkan SDK** - 硬件加速视频解码
- **FFmpeg 8.0.2** 预编译库 - 放置于 `multimedia/src/3rdparty/ffmpeg/`
- **JDK 17** - Android 构建所需

### Windows (LLVM-MinGW)

```powershell
# Debug 构建
cmake --preset llvm-mingw-debug
cmake --build --preset llvm-mingw-debug -j 8

# Release 构建
cmake --preset llvm-mingw-release
cmake --build --preset llvm-mingw-release -j 8
```

或使用脚本:

```powershell
.\build_windows.ps1        # Debug 增量构建
```

### Android (arm64-v8a)

```powershell
# Release 构建 + 生成 APK
.\build_arm64-v8a_apk.ps1

# 清除缓存重新构建
.\build_arm64-v8a_apk.ps1 -Clean
```

或手动步骤:

```powershell
cmake --preset android-arm64-release
cmake --build --preset android-arm64-release -j 8
```

## QML 模块

| 模块 URI            | 库                 | 说明                                  |
| ----------------- | ----------------- | ----------------------------------- |
| `qz.player`       | qzPlayer          | 播放器窗口、控件、播放列表                       |
| `qz.theme`        | qzTheme           | 主题系统 (平台感知)                         |
| `qz.controls.md3` | Md3Controls       | MD3 风格控件 (Button, Slider, Switch 等) |
| `qz.multimedia`   | qzMultimediaQuick | 多媒体 QML 绑定                          |

## 核心模块说明

### qzMultimedia

多媒体核心库，封装 FFmpeg 实现跨平台音视频播放。包含:

- **音频**: AudioInput / AudioOutput / AudioSink / AudioSource / AudioSpectrumAnalyzer
- **视频**: VideoSink / VideoFrame / VideoFrameFormat
- **播放**: MediaPlayer / PlaybackOptions / PreviewFrameProvider
- **平台**: WASAPI (Windows), AAudio (Android), FFmpeg 插件

### qzPlayer

播放器 UI 库，基于 QML 构建。支持:

- 桌面端 (PlayerWindow + QWindowKit 无边框窗口)
- 移动端 (PlayerWindow 移动适配)
- 播放列表、章节跳转、迷你模式、通知栏
- Vulkan 着色器 (圆角图片渲染)

### qzTheme

平台感知的主题系统，为 Windows / Android / Linux 提供不同的后端实现。

### qzLog

基于 C++20 Module 的日志库，封装 spdlog，支持 fmt 格式化。

## 第三方依赖

| 库                   | 版本      | 许可证           | 用途            |
| ------------------- | ------- | ------------- | ------------- |
| lunasvg             | 3.5.0   | MIT           | SVG 渲染        |
| QWindowKit          | 1.5.0   | Apache-2.0    | Windows 无边框窗口 |
| kissfft             | 131.2.0 | BSD-3-Clause  | FFT (音频频谱分析)  |
| signalsmith-stretch | -       | MIT           | 音频重采样         |
| tlsf                | -       | BSD-2-Clause  | 内存分配器 (TLSF)  |
| fmt                 | 12.1.0  | MIT           | 格式化库          |
| spdlog              | 1.17.0  | MIT           | 日志库           |

完整第三方许可证详情见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。

## License

[MIT License](LICENSE)

本项目使用了 Apache 2.0 许可的 QWindowKit 库，分发时需保留相关版权声明。详见 [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)。
