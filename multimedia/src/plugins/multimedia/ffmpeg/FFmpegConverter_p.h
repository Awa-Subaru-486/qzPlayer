#ifndef FFMPEGCONVERTER_P_H
#define FFMPEGCONVERTER_P_H

#include <QtCore/qtconfigmacros.h>
#include <qzMultimedia/private/MultimediaGlobal_p.h>

QT_BEGIN_NAMESPACE

class VideoFrameFormat;
class VideoFrame;

namespace ffmpeg {

// 视频帧转换器，用于视频帧格式转换和色彩空间转换。
::VideoFrame convertFrame(::VideoFrame &src, const VideoFrameFormat &dstFormat);

}

QT_END_NAMESPACE

#endif
