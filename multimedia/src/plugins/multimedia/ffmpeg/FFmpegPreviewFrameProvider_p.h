#ifndef FFMPEGPREVIEWFRAMEPROVIDER_P_H
#define FFMPEGPREVIEWFRAMEPROVIDER_P_H

#include <qzFFmpegMediaPluginImpl/private/FFmpeg_p.h>
#include <qzFFmpegMediaPluginImpl/private/FFmpegMediaDataHolder_p.h>  // ICancelToken
#include <qzMultimedia/private/PlatformPreviewFrameProvider_p.h>

#include <QtCore/qurl.h>
#include <QtCore/qmutex.h>
#include <QtConcurrent/QtConcurrent>

#include <atomic>
#include <memory>

QT_BEGIN_NAMESPACE

namespace ffmpeg {

// 预览帧提取的取消令牌
class CancelToken : public ICancelToken
{
public:
    bool isCancelled() const override { return m_cancelled.load(std::memory_order_acquire); }
    void cancel() { m_cancelled.store(true, std::memory_order_release); }
private:
    std::atomic_bool m_cancelled = false;
};

// FFmpeg 预览帧提供者
// 使用独立的 AVFormatContext + AVCodecContext 进行快速 seek + decode
// 保留 YUV 原始平面数据，避免 sws_scale 转换开销
//
// 线程安全设计（完全异步，不阻塞 GUI 线程）：
// - setSource / requestFrame / cancel 在主线程调用，仅操作原子变量和 shared_ptr
// - extractFrame 在工作线程执行，使用局部变量管理 FFmpeg context
// - 每次 requestFrame 创建独立的 context，不复用，避免 context 生命周期问题
// - token 通过 shared_ptr 共享，extractFrame 通过 interrupt callback 检测取消
class PreviewFrameProvider : public PlatformPreviewFrameProvider
{
public:
    PreviewFrameProvider();
    ~PreviewFrameProvider() override;

    void setSource(const QUrl &source) override;
    void requestFrame(qint64 positionMs, const QSize &maxSize,
                      std::function<void(const PreviewFrameData &)> callback) override;
    void cancel() override;

private:
    // 在工作线程中执行实际的帧提取
    // source 通过值传递，避免线程安全问题
    // context 使用局部变量，函数返回时自动释放
    static PreviewFrameData extractFrame(const QUrl &source, qint64 positionMs,
                                          const QSize &maxSize,
                                          const std::shared_ptr<CancelToken> &token);

    // 将 AVFrame 转换为 PreviewFrameData（保留 YUV 原始格式，零拷贝）
    static PreviewFrameData frameToPreviewData(const AVFrame *frame, const QSize &maxSize);

    // 设置 format context 的 interrupt callback
    static void setupInterruptCallback(AVFormatContext *fmtCtx,
                                        const std::shared_ptr<CancelToken> &token);

    // m_source 使用 mutex 保护，因为主线程写，工作线程读
    QMutex m_sourceMutex;
    QUrl m_source;

    // 取消令牌，通过 shared_ptr 共享给工作线程
    std::shared_ptr<CancelToken> m_cancelToken;

    // 当前待处理的任务（用于析构时等待）
    QFuture<void> m_pendingTask;
};

} // namespace ffmpeg

QT_END_NAMESPACE

#endif // FFMPEGPREVIEWFRAMEPROVIDER_P_H
