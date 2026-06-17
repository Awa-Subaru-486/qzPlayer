// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QUICKVIDEOOUTPUT_P_H
#define QUICKVIDEOOUTPUT_P_H

#include <QtCore/qrect.h>
#include <QtCore/qpointer.h>
#include <QtCore/qmutex.h>
#include <QtQuick/qquickitem.h>

#include <qzMultimedia/VideoFrame.h>
#include <qzMultimedia/VideoFrameFormat.h>
#include <qzMultimedia/VideoSink.h>
#include "qzmultimediaquickexports.h"
#include <rhi/qrhi.h>

#include <thread>

QT_BEGIN_NAMESPACE

class QuickVideoBackend;
class VideoOutputOrientationHandler;
class VideoSink;
class QSGVideoNode;
class VideoFrameTexturePool;
using QVideoFrameTexturePoolWPtr = std::weak_ptr<VideoFrameTexturePool>;

// QML 视频输出槽：VideoSink 的 QML 封装
class QuickVideoSink : public VideoSink
{
    Q_OBJECT
public:
    explicit QuickVideoSink(QObject *parent = nullptr) : VideoSink(parent)
    {
        connect(this, &VideoSink::videoFrameChanged, this, &QuickVideoSink::videoFrameChanged,
                Qt::DirectConnection);
    }

Q_SIGNALS:
    void videoFrameChanged();
};

// QML 视频输出：在 QML 场景中渲染视频帧
class QZ_MULTIMEDIAQUICK_EXPORT QuickVideoOutput : public QQuickItem
{
    Q_OBJECT
    Q_DISABLE_COPY(QuickVideoOutput)
    Q_PROPERTY(FillMode fillMode READ fillMode WRITE setFillMode NOTIFY fillModeChanged)
    Q_PROPERTY(EndOfStreamPolicy endOfStreamPolicy READ endOfStreamPolicy WRITE setEndOfStreamPolicy
                       NOTIFY endOfStreamPolicyChanged REVISION(6, 9))
    Q_PROPERTY(int orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool mirrored READ mirrored WRITE setMirrored NOTIFY mirroredChanged REVISION(6, 9))
    Q_PROPERTY(QRectF sourceRect READ sourceRect NOTIFY sourceRectChanged)
    Q_PROPERTY(QRectF contentRect READ contentRect NOTIFY contentRectChanged)
    Q_PROPERTY(VideoSink* videoSink READ videoSink CONSTANT)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_MOC_INCLUDE(VideoSink.h)
    Q_MOC_INCLUDE(VideoFrame.h)

public:
    // 填充模式：拉伸、保持比例、裁剪
    enum FillMode
    {
        Stretch            = Qt::IgnoreAspectRatio,
        PreserveAspectFit  = Qt::KeepAspectRatio,
        PreserveAspectCrop = Qt::KeepAspectRatioByExpanding
    };
    Q_ENUM(FillMode)

    // 流结束策略：清除输出或保持最后一帧
    enum EndOfStreamPolicy
    {
        ClearOutput,
        KeepLastFrame
    };
    Q_ENUM(EndOfStreamPolicy)

    explicit QuickVideoOutput(QQuickItem *parent = nullptr);
    ~QuickVideoOutput() override;

    // 视频输出槽
    Q_INVOKABLE VideoSink *videoSink() const;

    // 填充模式
    FillMode fillMode() const;
    void setFillMode(FillMode mode);

    // 方向和镜像
    int orientation() const;
    void setOrientation(int);

    bool mirrored() const;
    void setMirrored(bool);

    qreal radius() const;
    void setRadius(qreal);

    // 源矩形和内容矩形
    QRectF sourceRect() const;
    QRectF contentRect() const;

    // 流结束策略
    EndOfStreamPolicy endOfStreamPolicy() const;
    void setEndOfStreamPolicy(EndOfStreamPolicy policy);

    // 清除输出
    Q_REVISION(6, 9) Q_INVOKABLE void clearOutput();

Q_SIGNALS:
    void sourceChanged();
    void fillModeChanged(QuickVideoOutput::FillMode);
    void orientationChanged();
    void mirroredChanged();
    void sourceRectChanged();
    void contentRectChanged();
    void endOfStreamPolicyChanged(QuickVideoOutput::EndOfStreamPolicy);
    void radiusChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void itemChange(ItemChange change, const ItemChangeData &changeData) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void releaseResources() override;

private:
    QSize nativeSize() const;
    void updateGeometry();
    QRectF adjustedViewport() const;

    void setFrame(const VideoFrame &frame);

    void initRhiForSink();
    void updateHdr(QSGVideoNode *videoNode);
    void disconnectWindowConnections();

private Q_SLOTS:
    void _q_newFrame(QSize);
    void _q_updateGeometry();

private:

    QSize m_nativeSize;

    bool m_geometryDirty = true;
    QRectF m_lastRect;
    QRectF m_contentRect;
    int m_orientation = 0;
    bool m_mirrored = false;
    QtVideo::Rotation m_frameDisplayingRotation = QtVideo::Rotation::None;
    Qt::AspectRatioMode m_aspectRatioMode = Qt::KeepAspectRatio;

    QPointer<QQuickWindow> m_window;
    VideoSink *m_sink = nullptr;
    VideoFrameFormat m_videoFormat;

    QVideoFrameTexturePoolWPtr m_texturePool;
    VideoFrame m_frame;
    bool m_frameChanged = false;
    bool m_subtitleChanged = false;
    bool m_subtitleStyleChanged = false;
    QMutex m_frameMutex;
    QRectF m_renderedRect;
    QRectF m_sourceTextureRect;

    EndOfStreamPolicy m_endOfStreamPolicy = ClearOutput;

    qreal m_radius = 0.0;

    QRhiSwapChain::Format m_lastSwapChainFormat = QRhiSwapChain::SDR;

    struct DestructorGuard
    {
        QMutex m_mutex;
        bool m_isAlive{ true };

        template <typename Functor>
        void runWhileAlive(Functor &&f)
        {
            QMutexLocker lock(&m_mutex);
            if (m_isAlive)
                f();
        }
    };

    std::shared_ptr<DestructorGuard> m_destructorGuard = std::make_shared<DestructorGuard>();

    template <typename Functor>
    auto makeGuardedCall(Functor &&f)
    {
        return [f = std::forward<Functor>(f), guard = m_destructorGuard](auto... params) {
            if (g_signalBackoff) {
                Q_UNLIKELY_BRANCH;
                std::this_thread::sleep_for(*g_signalBackoff);
            }

            guard->runWhileAlive([&] {
                f(params...);
            });
        };
    }

    static std::optional<std::chrono::nanoseconds> g_signalBackoff;

public:
    static void setSignalBackoff(std::optional<std::chrono::nanoseconds>);
};

QT_END_NAMESPACE

#endif
