#ifndef PLAYBACKENGINE_FFMPEGPLAYBACKENGINEDEFS_P_H
#define PLAYBACKENGINE_FFMPEGPLAYBACKENGINEDEFS_P_H

#include "qobject.h"
#include "qpointer.h"

#include <memory>
#include <array>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
class PlaybackEngine;
}

namespace ffmpeg {

// 流索引数组类型
using StreamIndexes = std::array<int, 3>;

// 播放引擎对象控制器
class PlaybackEngineObjectsController;
// 播放引擎对象基类
class PlaybackEngineObject;
// 解复用器
class Demuxer;
// 流解码器
class StreamDecoder;
// 渲染器基类
class Renderer;
// 字幕渲染器
class SubtitleRenderer;
// 音频渲染器
class AudioRenderer;
// 视频渲染器
class VideoRenderer;

}

QT_END_NAMESPACE

#endif
