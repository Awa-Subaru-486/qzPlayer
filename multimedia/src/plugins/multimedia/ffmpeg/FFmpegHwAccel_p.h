#ifndef FFMPEGHWACCEL_P_H
#define FFMPEGHWACCEL_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegTextureConverter_p.h>
#include "VideoFrameFormat.h"
#include <qzMultimedia/private/HwVideoBuffer_p.h>
#include <qzMultimedia/private/RhiValueMapper_p.h>

#include <qshareddata.h>
#include <memory>
#include <functional>
#include <mutex>

QT_BEGIN_NAMESPACE

class QRhi;
class QRhiTexture;

namespace ffmpeg {
class VideoBuffer;

AVPixelFormat getFormat(AVCodecContext *codecContext, const AVPixelFormat *fmt);

class HWAccel;

using HWAccelUPtr = std::unique_ptr<HWAccel>;

struct HwFrameContextData
{
    using AVHWFramesContextDeleter = void (*)(struct AVHWFramesContext *ctx);
    AVHWFramesContextDeleter avDeleter = nullptr;
    void *avUserOpaque = nullptr;

    RhiValueMapper<TextureConverter> textureConverterMapper = {};

    static HwFrameContextData &ensure(AVFrame &hwFrame);
};

// 硬件加速基类，提供设备创建和管理功能
class HWAccel
{
    AVBufferUPtr m_hwDeviceContext;
    AVBufferUPtr m_hwFramesContext;

    mutable std::once_flag m_constraintsOnceFlag;
    mutable AVHWFramesConstraintsUPtr m_constraints;

    bool m_usesExternalDevice = false;

    struct Pr{};
public:
    explicit HWAccel(Pr, AVBufferUPtr hwDeviceContext, bool usesExternalDevice = false)
    : m_hwDeviceContext(std::move(hwDeviceContext))
    , m_usesExternalDevice(usesExternalDevice)
    { }
    ~HWAccel();

    static HWAccelUPtr create(AVHWDeviceType deviceType);

    static HWAccelUPtr createWithExternalDevice(AVHWDeviceType deviceType, QRhi *rhi);

    static std::pair<std::optional<Codec>, HWAccelUPtr> findDecoderWithHwAccel(AVCodecID id);

    static std::pair<std::optional<Codec>, HWAccelUPtr> findDecoderWithHwAccel(AVCodecID id, AVHWDeviceType hwDeviceType);

    static std::pair<std::optional<Codec>, HWAccelUPtr> findDecoderWithHwAccel(AVCodecID id, AVHWDeviceType hwDeviceType, QRhi *rhi);

    AVHWDeviceType deviceType() const;

    AVBufferRef *hwDeviceContextAsBuffer() const { return m_hwDeviceContext.get(); }
    AVHWDeviceContext *hwDeviceContext() const;
    AVPixelFormat hwFormat() const;
    const AVHWFramesConstraints *constraints() const;

    bool matchesSizeContraints(QSize size) const;

    bool usesExternalDevice() const { return m_usesExternalDevice; }

    void createFramesContext(AVPixelFormat swFormat, const QSize &size);
    AVBufferRef *hwFramesContextAsBuffer() const { return m_hwFramesContext.get(); }
    AVHWFramesContext *hwFramesContext() const;

    static AVPixelFormat format(AVFrame *frame);
    static const std::vector<AVHWDeviceType> &encodingDeviceTypes();

    static const std::vector<AVHWDeviceType> &decodingDeviceTypes();

private:

};

AVFrameUPtr copyFromHwPool(AVFrameUPtr frame);

}

QT_END_NAMESPACE

#endif
