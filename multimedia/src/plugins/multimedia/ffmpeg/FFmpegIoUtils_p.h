#ifndef FFMPEGIOUTILS_P_H
#define FFMPEGIOUTILS_P_H

#include "MultimediaGlobal.h"
#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>

#include <type_traits>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// IO 工具函数集，提供 QIODevice 与 FFmpeg IO 的适配功能。
int readQIODevice(void *opaque, uint8_t *buf, int buf_size);

using AvioWriteBufferType =
        std::conditional_t<QT_FFMPEG_AVIO_WRITE_CONST, const uint8_t *, uint8_t *>;

int writeQIODevice(void *opaque, AvioWriteBufferType buf, int buf_size);

int64_t seekQIODevice(void *opaque, int64_t offset, int whence);

}

QT_END_NAMESPACE

#endif
