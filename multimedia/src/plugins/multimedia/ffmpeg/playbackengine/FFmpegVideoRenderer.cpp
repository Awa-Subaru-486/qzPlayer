

#include "playbackengine/FFmpegVideoRenderer_p.h"
#include "FFmpegVideoBuffer_p.h"
#include "VideoSink.h"
#include "private/VideoFrame_p.h"

QT_BEGIN_NAMESPACE

namespace ffmpeg {

VideoRenderer::VideoRenderer(const PlaybackEngineObjectID &id, const TimeController &tc,
                             ::VideoSink *sink, const VideoTransformation &transform)
    : Renderer(id, tc), m_sink(sink), m_transform(transform)
{
    // =========================================================================
    // 视频渲染器构造函数
    // 初始化视频输出接收器和视频变换
    // =========================================================================
}

void VideoRenderer::setOutput(::VideoSink *sink, bool cleanPrevSink)
{
    // =========================================================================
    // 设置视频输出接收器
    // 参数: sink - 新的视频接收器
    //       cleanPrevSink - 是否清除旧接收器的帧
    // =========================================================================
    setOutputInternal(m_sink, sink, [=](::VideoSink *prev) {
        if (!prev)
            return;

        // 如果新接收器存在，将旧帧传递给它
        if (sink)
            sink->setVideoFrame(prev->videoFrame());

        // 如果需要清除旧接收器，清空其帧
        if (cleanPrevSink)
            prev->setVideoFrame({});
    });
}

// 视频帧渲染实现。
// 与音频渲染器不同，视频渲染更简单，因为:
// 1. 视频不需要缓冲管理(帧立即显示)
// 2. 视频跟随音频设置的时间线(通过共享的 TimeController)
// 3. 不需要同步反馈循环 - 视频只需按时显示
//
// 时序由基类 Renderer 处理:
// - nextTimePoint() 返回此帧应该显示的时间
// - 调度器在正确的时间调用 doNextStep()
// - 此函数只需转换帧并发送到 VideoSink
VideoRenderer::RenderingResult VideoRenderer::renderInternal(Frame frame)
{
    if (!m_sink)
        return {};

    if (!frame.isValid()) {
        m_sink->setVideoFrame({});
        return {};
    }

    s_lastVideoPtsUs.storeRelease(frame.absolutePts().get());

    const auto codecContext = frame.codecContext();
    Q_ASSERT(codecContext);

    // 计算像素宽高比
    const auto pixelAspectRatio = codecContext->pixelAspectRatio(frame.avFrame());

    // 创建视频缓冲
    auto buffer = std::make_unique<VideoBuffer>(frame.takeAVFrame(), pixelAspectRatio);

    // 创建视频帧格式
    VideoFrameFormat format(buffer->size(), buffer->pixelFormat());

    // 设置颜色空间属性
    format.setColorSpace(buffer->colorSpace());
    format.setColorTransfer(buffer->colorTransfer());
    format.setColorRange(buffer->colorRange());
    format.setMaxLuminance(buffer->maxNits());

    // 设置视频变换(旋转和镜像)
    format.setRotation(m_transform.rotation);
    format.setMirrored(m_transform.mirroredHorizontallyAfterRotation);

    // 创建视频帧
    ::VideoFrame videoFrame = VideoFramePrivate::createFrame(std::move(buffer), format);

    // 设置帧时间范围
    videoFrame.setStartTime(frame.startTime().get());
    videoFrame.setEndTime(frame.endTime().get());

    // 将视频帧发送到接收器
    m_sink->setVideoFrame(videoFrame);

    return {};
}

}

QT_END_NAMESPACE

#include "moc_FFmpegVideoRenderer_p.cpp"
