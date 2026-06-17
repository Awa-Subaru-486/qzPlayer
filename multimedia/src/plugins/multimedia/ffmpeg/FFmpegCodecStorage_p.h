#ifndef FFMPEGCODECSTORAGE_P_H
#define FFMPEGCODECSTORAGE_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpegDefs_p.h>
#include "MultimediaGlobal.h"

#include <functional>
#include <optional>

QT_BEGIN_NAMESPACE

namespace ffmpeg {
// 编解码器存储和管理类，负责编解码器的查找和初始化。
class Codec;

bool findAndOpenAVDecoder(AVCodecID codecId,
                          const std::function<AVScore(const Codec &)> &scoresGetter,
                          const std::function<bool(const Codec &)> &codecOpener);

bool findAndOpenAVEncoder(AVCodecID codecId,
                          const std::function<AVScore(const Codec &)> &scoresGetter,
                          const std::function<bool(const Codec &)> &codecOpener);

std::optional<Codec> findAVDecoder(AVCodecID codecId,
                                   const std::optional<PixelOrSampleFormat> &format = {});

std::optional<Codec> findAVSoftwareDecoder(AVCodecID codecId, const std::optional<PixelOrSampleFormat> &format = {});

std::optional<Codec> findAVEncoder(AVCodecID codecId, const std::optional<PixelOrSampleFormat> &format = {});

}

QT_END_NAMESPACE

#endif
