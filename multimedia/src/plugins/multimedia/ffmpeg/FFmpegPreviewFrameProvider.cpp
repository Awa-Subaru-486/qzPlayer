#include "FFmpegPreviewFrameProvider_p.h"

import qzLog;

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
}

QT_BEGIN_NAMESPACE

namespace ffmpeg {

static qz::Log::LogCategory qLcPreviewFrame("qz.multimedia.ffmpeg.previewframe");

// ============================================================
// 辅助：自定义 buffer 释放器，管理 YUV 平面数据的生命周期
// ============================================================

struct FrameBufferDeleter
{
    void operator()(uchar *ptr) const
    {
        if (ptr)
            av_free(ptr);
    }
};

using FrameBufferPtr = std::shared_ptr<const uchar>;

// ============================================================
// PreviewFrameProvider 实现
// ============================================================

PreviewFrameProvider::PreviewFrameProvider() = default;

PreviewFrameProvider::~PreviewFrameProvider()
{
    // 析构时取消并等待工作线程退出
    // 此时工作线程使用的是局部 context，等待它退出即可，无需清理成员 context
    cancel();
    if (m_pendingTask.isRunning())
        m_pendingTask.waitForFinished();
}

void PreviewFrameProvider::setSource(const QUrl &source)
{
    // 完全异步：仅更新 m_source，不阻塞 GUI 线程
    // 工作线程通过 requestFrame 传入的 source 副本工作，不受 m_source 变化影响
    QMutexLocker locker(&m_sourceMutex);
    m_source = source;
}

void PreviewFrameProvider::requestFrame(qint64 positionMs, const QSize &maxSize,
                                        std::function<void(const PreviewFrameData &)> callback)
{
    // 取消之前的请求（非阻塞）
    cancel();

    // 创建新 token
    m_cancelToken = std::make_shared<CancelToken>();

    // 读取当前 source（加锁）
    QUrl source;
    {
        QMutexLocker locker(&m_sourceMutex);
        source = m_source;
    }

    if (source.isEmpty())
    {
        callback(PreviewFrameData{});
        return;
    }

    auto token = m_cancelToken;

    // 在线程池中执行帧提取
    // source 通过值传递，token 通过 shared_ptr 共享
    // extractFrame 使用局部变量管理 context，函数返回时自动释放
    m_pendingTask = QtConcurrent::run([this, source, positionMs, maxSize, token, callback]() {
        if (token->isCancelled())
        {
            callback(PreviewFrameData{});
            return;
        }

        PreviewFrameData data = extractFrame(source, positionMs, maxSize, token);
        callback(data);
    });
}

void PreviewFrameProvider::cancel()
{
    if (m_cancelToken)
        m_cancelToken->cancel();
}

PreviewFrameData PreviewFrameProvider::extractFrame(const QUrl &source, qint64 positionMs,
                                                     const QSize &maxSize,
                                                     const std::shared_ptr<CancelToken> &token)
{
    if (source.isEmpty())
        return {};

    // 所有 context 使用局部变量，函数返回时自动释放
    // 这样 setSource 无需等待工作线程，完全异步
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *codecCtx = nullptr;
    int videoStreamIndex = -1;

    // RAII 确保退出时清理
    auto cleanup = [&]() {
        if (codecCtx)
            avcodec_free_context(&codecCtx);
        if (fmtCtx)
            avformat_close_input(&fmtCtx);
    };

    const QByteArray urlBytes = source.toString(QUrl::PreferLocalFile).toUtf8();

    fmtCtx = avformat_alloc_context();
    if (!fmtCtx)
        return {};

    // 设置 interrupt callback，token 通过 shared_ptr 保持生命周期
    setupInterruptCallback(fmtCtx, token);

    AVDictionary *options = nullptr;
    av_dict_set(&options, "probesize", "32768", 0);
    av_dict_set(&options, "analyzeduration", "100000", 0);

    if (avformat_open_input(&fmtCtx, urlBytes.constData(), nullptr, &options) < 0)
    {
        av_dict_free(&options);
        avformat_free_context(fmtCtx);
        return {};
    }
    av_dict_free(&options);
    // open_input 可能重置 interrupt_callback，重新设置
    setupInterruptCallback(fmtCtx, token);

    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i)
    {
        if (fmtCtx->streams[i]->codecpar->codec_type != AVMEDIA_TYPE_VIDEO)
            fmtCtx->streams[i]->discard = AVDISCARD_ALL;
    }

    if (avformat_find_stream_info(fmtCtx, nullptr) < 0)
    {
        cleanup();
        return {};
    }

    for (unsigned int i = 0; i < fmtCtx->nb_streams; ++i)
    {
        const AVStream *stream = fmtCtx->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            && !(stream->disposition & AV_DISPOSITION_ATTACHED_PIC))
        {
            videoStreamIndex = static_cast<int>(i);
            break;
        }
    }

    if (videoStreamIndex < 0)
    {
        cleanup();
        return {};
    }

    const AVStream *videoStream = fmtCtx->streams[videoStreamIndex];
    const AVCodec *codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec)
    {
        cleanup();
        return {};
    }

    codecCtx = avcodec_alloc_context3(codec);
    if (!codecCtx)
    {
        cleanup();
        return {};
    }

    if (avcodec_parameters_to_context(codecCtx, videoStream->codecpar) < 0)
    {
        cleanup();
        return {};
    }

    codecCtx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    codecCtx->flags2 |= AV_CODEC_FLAG2_FAST;

    if (avcodec_open2(codecCtx, codec, nullptr) < 0)
    {
        cleanup();
        return {};
    }

    if (token && token->isCancelled())
    {
        cleanup();
        return {};
    }

    // 钳制 position 到有效范围 [0, duration]
    const int64_t durationUs = fmtCtx->duration;  // 微秒，AV_TIME_BASE
    int64_t seekTargetUs = positionMs * 1000;     // ms -> us
    if (seekTargetUs < 0)
        seekTargetUs = 0;
    if (durationUs > 0 && seekTargetUs > durationUs)
        seekTargetUs = durationUs;

    // 使用流 time_base 进行 seek
    const AVRational streamTimeBase = fmtCtx->streams[videoStreamIndex]->time_base;
    const int64_t seekPos = av_rescale_q(seekTargetUs, { 1, AV_TIME_BASE }, streamTimeBase);

    if (av_seek_frame(fmtCtx, videoStreamIndex, seekPos, AVSEEK_FLAG_BACKWARD) < 0)
    {
        if (av_seek_frame(fmtCtx, -1, seekTargetUs, AVSEEK_FLAG_BACKWARD) < 0)
        {
            cleanup();
            return {};
        }
    }

    avcodec_flush_buffers(codecCtx);

    AVFrameUPtr frame = makeAVFrame();
    AVPacketUPtr packet(av_packet_alloc());
    if (!packet)
    {
        cleanup();
        return {};
    }

    AVFrameUPtr bestFrame = makeAVFrame();
    bool hasBestFrame = false;
    int64_t bestDiff = INT64_MAX;

    constexpr int maxPackets = 50;
    int packetCount = 0;

    while (packetCount < maxPackets && av_read_frame(fmtCtx, packet.get()) >= 0)
    {
        if (token && token->isCancelled())
        {
            cleanup();
            return {};
        }

        if (packet->stream_index != videoStreamIndex)
        {
            av_packet_unref(packet.get());
            continue;
        }

        packetCount++;

        int ret = avcodec_send_packet(codecCtx, packet.get());
        av_packet_unref(packet.get());
        if (ret < 0)
            continue;

        while (avcodec_receive_frame(codecCtx, frame.get()) >= 0)
        {
            if (token && token->isCancelled())
            {
                cleanup();
                return {};
            }

            const AVStream *stream = fmtCtx->streams[videoStreamIndex];
            int64_t frameTimeUs = 0;
            if (frame->best_effort_timestamp != AV_NOPTS_VALUE)
            {
                frameTimeUs = av_rescale_q(frame->best_effort_timestamp, stream->time_base,
                                           { 1, AV_TIME_BASE });
            }
            else if (frame->pts != AV_NOPTS_VALUE)
            {
                frameTimeUs = av_rescale_q(frame->pts, stream->time_base,
                                           { 1, AV_TIME_BASE });
            }

            const int64_t diff = frameTimeUs - seekTargetUs;
            const int64_t absDiff = diff < 0 ? -diff : diff;

            if (absDiff < bestDiff)
            {
                bestDiff = absDiff;
                av_frame_move_ref(bestFrame.get(), frame.get());
                hasBestFrame = true;
            }

            av_frame_unref(frame.get());

            if (diff >= 0 && absDiff < 100000)
                break;
        }

        if (hasBestFrame && bestDiff < 100000)
            break;
    }

    if (!hasBestFrame)
    {
        cleanup();
        return {};
    }

    PreviewFrameData result = frameToPreviewData(bestFrame.get(), maxSize);
    cleanup();
    return result;
}

void PreviewFrameProvider::setupInterruptCallback(AVFormatContext *fmtCtx,
                                                   const std::shared_ptr<CancelToken> &token)
{
    if (!fmtCtx)
        return;

    if (!token)
    {
        fmtCtx->interrupt_callback.opaque = nullptr;
        fmtCtx->interrupt_callback.callback = nullptr;
        return;
    }

    // token 通过 shared_ptr 保持生命周期，opaque 指向 token.get()
    // 由于 extractFrame 使用局部变量，token 的生命周期由 lambda 捕获保证
    fmtCtx->interrupt_callback.opaque = token.get();
    fmtCtx->interrupt_callback.callback = [](void *opaque) {
        const auto *t = static_cast<const ICancelToken *>(opaque);
        return (t && t->isCancelled()) ? 1 : 0;
    };
}

PreviewFrameData PreviewFrameProvider::frameToPreviewData(const AVFrame *frame, const QSize &maxSize)
{
    if (!frame || !frame->data[0])
        return {};

    const int srcW = frame->width;
    const int srcH = frame->height;
    const auto srcFmt = static_cast<AVPixelFormat>(frame->format);

    PreviewFrameData data;
    data.width = srcW;
    data.height = srcH;

    if (srcFmt == AV_PIX_FMT_YUV420P || srcFmt == AV_PIX_FMT_YUVJ420P)
    {
        data.format = PreviewFrameData::Format::YUV420P;

        const int uvW = (srcW + 1) / 2;
        const int uvH = (srcH + 1) / 2;

        // Y 平面
        {
            const int ySize = frame->linesize[0] * srcH;
            uchar *buf = static_cast<uchar *>(av_malloc(ySize));
            if (!buf)
                return {};
            memcpy(buf, frame->data[0], ySize);
            data.planes[0].data = FrameBufferPtr(buf, FrameBufferDeleter{});
            data.planes[0].linesize = frame->linesize[0];
            data.planes[0].width = srcW;
            data.planes[0].height = srcH;
        }

        // U 平面
        {
            const int uSize = frame->linesize[1] * uvH;
            uchar *buf = static_cast<uchar *>(av_malloc(uSize));
            if (!buf)
                return {};
            memcpy(buf, frame->data[1], uSize);
            data.planes[1].data = FrameBufferPtr(buf, FrameBufferDeleter{});
            data.planes[1].linesize = frame->linesize[1];
            data.planes[1].width = uvW;
            data.planes[1].height = uvH;
        }

        // V 平面
        {
            const int vSize = frame->linesize[2] * uvH;
            uchar *buf = static_cast<uchar *>(av_malloc(vSize));
            if (!buf)
                return {};
            memcpy(buf, frame->data[2], vSize);
            data.planes[2].data = FrameBufferPtr(buf, FrameBufferDeleter{});
            data.planes[2].linesize = frame->linesize[2];
            data.planes[2].width = uvW;
            data.planes[2].height = uvH;
        }

        return data;
    }
    else if (srcFmt == AV_PIX_FMT_NV12)
    {
        data.format = PreviewFrameData::Format::NV12;

        const int uvW = (srcW + 1) / 2;
        const int uvH = (srcH + 1) / 2;

        // Y 平面
        {
            const int ySize = frame->linesize[0] * srcH;
            uchar *buf = static_cast<uchar *>(av_malloc(ySize));
            if (!buf)
                return {};
            memcpy(buf, frame->data[0], ySize);
            data.planes[0].data = FrameBufferPtr(buf, FrameBufferDeleter{});
            data.planes[0].linesize = frame->linesize[0];
            data.planes[0].width = srcW;
            data.planes[0].height = srcH;
        }

        // UV 交织平面
        {
            const int uvSize = frame->linesize[1] * uvH;
            uchar *buf = static_cast<uchar *>(av_malloc(uvSize));
            if (!buf)
                return {};
            memcpy(buf, frame->data[1], uvSize);
            data.planes[1].data = FrameBufferPtr(buf, FrameBufferDeleter{});
            data.planes[1].linesize = frame->linesize[1];
            data.planes[1].width = uvW;
            data.planes[1].height = uvH;
        }

        return data;
    }
    else
    {
        // 其他格式：使用 sws_scale 转换为 RGBA
        const SwsContextUPtr swsCtx = createSwsContext(
            QSize(srcW, srcH), srcFmt,
            QSize(srcW, srcH), AV_PIX_FMT_RGBA,
            SWS_BILINEAR);

        if (!swsCtx)
            return {};

        AVFrameUPtr dstFrame = makeAVFrame();
        dstFrame->width = srcW;
        dstFrame->height = srcH;
        dstFrame->format = AV_PIX_FMT_RGBA;

        if (av_frame_get_buffer(dstFrame.get(), 32) < 0)
            return {};

        sws_scale(swsCtx.get(),
                  frame->data, frame->linesize, 0, srcH,
                  dstFrame->data, dstFrame->linesize);

        data.format = PreviewFrameData::Format::RGBA;

        const int bufSize = dstFrame->linesize[0] * srcH;
        uchar *buf = static_cast<uchar *>(av_malloc(bufSize));
        if (!buf)
            return {};
        memcpy(buf, dstFrame->data[0], bufSize);

        data.planes[0].data = FrameBufferPtr(buf, FrameBufferDeleter{});
        data.planes[0].linesize = dstFrame->linesize[0];
        data.planes[0].width = srcW;
        data.planes[0].height = srcH;

        return data;
    }
}

} // namespace ffmpeg

QT_END_NAMESPACE
