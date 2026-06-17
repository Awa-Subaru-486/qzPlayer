#include "WindowsFormatInfo_p.h"

#include <mfapi.h>
#include <mftransform.h>
#include <QtCore/private/qcomptr_p.h>
#include <private/WindowsMultimediaUtils_p.h>
#include <private/ComTaskResource_p.h>

#include <QtCore/qlist.h>
#include <QtCore/qset.h>
#include <QtCore/qhash.h>

namespace {

template<typename T>
using CheckedCodecs = QHash<std::pair<T, MediaFormat::ConversionMode>, bool>;

bool isSupportedMFT(const GUID &category, const MFT_REGISTER_TYPE_INFO &type, MediaFormat::ConversionMode mode)
{
    UINT32 count = 0;
    ComTaskResource<IMFActivate *> activate;
    HRESULT hr = MFTEnumEx(
            category,
            MFT_ENUM_FLAG_ALL,
            (mode == MediaFormat::Encode) ? nullptr : &type,
            (mode == MediaFormat::Encode) ? &type : nullptr,
            activate.address(),
            &count
            );

    return SUCCEEDED(hr) && count > 0;
}

bool isSupportedCodec(MediaFormat::AudioCodec codec, MediaFormat::ConversionMode mode)
{
    return isSupportedMFT((mode == MediaFormat::Encode) ? MFT_CATEGORY_AUDIO_ENCODER : MFT_CATEGORY_AUDIO_DECODER,
                          { MFMediaType_Audio, WindowsMultimediaUtils::audioFormatForCodec(codec) },
                          mode);
}

bool isSupportedCodec(MediaFormat::VideoCodec codec, MediaFormat::ConversionMode mode)
{
    return isSupportedMFT((mode == MediaFormat::Encode) ? MFT_CATEGORY_VIDEO_ENCODER : MFT_CATEGORY_VIDEO_DECODER,
                          { MFMediaType_Video, WindowsMultimediaUtils::videoFormatForCodec(codec) },
                          mode);
}

template <typename T>
bool isSupportedCodec(T codec, MediaFormat::ConversionMode m, CheckedCodecs<T> &checkedCodecs)
{
    if (auto it = checkedCodecs.constFind(std::pair{codec, m}); it != checkedCodecs.constEnd())
        return it.value();

    const bool supported = isSupportedCodec(codec, m);

    checkedCodecs.insert(std::pair{codec, m}, supported);
    return supported;
}

}

namespace windows {

FormatInfo::FormatInfo()
{
    const QList<CodecMap> containerTable = {
        { MediaFormat::MPEG4,
          { MediaFormat::AudioCodec::AAC, MediaFormat::AudioCodec::MP3, MediaFormat::AudioCodec::ALAC, MediaFormat::AudioCodec::AC3, MediaFormat::AudioCodec::EAC3 },
          { MediaFormat::VideoCodec::H264, MediaFormat::VideoCodec::H265, MediaFormat::VideoCodec::MotionJPEG } },
        { MediaFormat::Matroska,
          { MediaFormat::AudioCodec::AAC, MediaFormat::AudioCodec::MP3, MediaFormat::AudioCodec::ALAC, MediaFormat::AudioCodec::AC3, MediaFormat::AudioCodec::EAC3, MediaFormat::AudioCodec::FLAC, MediaFormat::AudioCodec::Vorbis, MediaFormat::AudioCodec::Opus },
          { MediaFormat::VideoCodec::H264, MediaFormat::VideoCodec::H265, MediaFormat::VideoCodec::VP8, MediaFormat::VideoCodec::VP9, MediaFormat::VideoCodec::MotionJPEG } },
        { MediaFormat::WebM,
          { MediaFormat::AudioCodec::Vorbis, MediaFormat::AudioCodec::Opus },
          { MediaFormat::VideoCodec::VP8, MediaFormat::VideoCodec::VP9 } },
        { MediaFormat::QuickTime,
          { MediaFormat::AudioCodec::AAC, MediaFormat::AudioCodec::MP3, MediaFormat::AudioCodec::ALAC, MediaFormat::AudioCodec::AC3, MediaFormat::AudioCodec::EAC3 },
          { MediaFormat::VideoCodec::H264, MediaFormat::VideoCodec::H265, MediaFormat::VideoCodec::MotionJPEG } },
        { MediaFormat::AAC,
          { MediaFormat::AudioCodec::AAC },
          {} },
        { MediaFormat::MP3,
          { MediaFormat::AudioCodec::MP3 },
          {} },
        { MediaFormat::FLAC,
          { MediaFormat::AudioCodec::FLAC },
          {} },
        { MediaFormat::Mpeg4Audio,
          { MediaFormat::AudioCodec::AAC, MediaFormat::AudioCodec::MP3, MediaFormat::AudioCodec::ALAC, MediaFormat::AudioCodec::AC3, MediaFormat::AudioCodec::EAC3 },
          {} },
        { MediaFormat::WMA,
          { MediaFormat::AudioCodec::WMA },
          {} },
        { MediaFormat::WMV,
          { MediaFormat::AudioCodec::WMA },
          { MediaFormat::VideoCodec::WMV } }
    };

    const QSet<MediaFormat::FileFormat> decoderFormats = {
        MediaFormat::MPEG4,
        MediaFormat::Matroska,
        MediaFormat::WebM,
        MediaFormat::QuickTime,
        MediaFormat::AAC,
        MediaFormat::MP3,
        MediaFormat::FLAC,
        MediaFormat::Mpeg4Audio,
        MediaFormat::WMA,
        MediaFormat::WMV,
    };

    const QSet<MediaFormat::FileFormat> encoderFormats = {
        MediaFormat::MPEG4,
        MediaFormat::AAC,
        MediaFormat::MP3,
        MediaFormat::FLAC,
        MediaFormat::Mpeg4Audio,
        MediaFormat::WMA,
        MediaFormat::WMV,
    };

    CheckedCodecs<MediaFormat::AudioCodec> checkedAudioCodecs;
    CheckedCodecs<MediaFormat::VideoCodec> checkedVideoCodecs;

    auto ensureCodecs = [&] (CodecMap &codecs, MediaFormat::ConversionMode mode) {
        codecs.audio.removeIf([&] (auto codec) { return !isSupportedCodec(codec, mode, checkedAudioCodecs); });
        codecs.video.removeIf([&] (auto codec) { return !isSupportedCodec(codec, mode, checkedVideoCodecs); });
        return !codecs.video.empty() || !codecs.audio.empty();
    };

    for (const auto &codecMap : containerTable) {
        if (decoderFormats.contains(codecMap.format)) {
            auto m = codecMap;
            if (ensureCodecs(m, MediaFormat::Decode))
                decoders.append(m);
        }

        if (encoderFormats.contains(codecMap.format)) {
            auto m = codecMap;
            if (ensureCodecs(m, MediaFormat::Encode))
                encoders.append(m);
        }
    }
}

FormatInfo::~FormatInfo()
{
}

}
