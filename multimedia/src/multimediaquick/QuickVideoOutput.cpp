// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "QuickVideoOutput_p.h"

#include <private/VideoOutputOrientationHandler_p.h>
#include <private/VideoFrameTexturePool_p.h>
#include <private/PlatformVideoSink_p.h>
#include <qzMultimedia/MediaPlayer.h>
#include <private/qfactoryloader_p.h>
#include <QtQuick/QQuickWindow>
#include <private/qquickwindow_p.h>
#include <private/MultimediaUtils_p.h>
#include <SGVideoNode_p.h>
#include <QtCore/qrunnable.h>

import qzLog;

QT_BEGIN_NAMESPACE

static qz::Log::LogCategory qLcVideo("qz.multimedia.video");

namespace {

inline bool qIsDefaultAspect(int o)
{
    return (o % 180) == 0;
}

inline bool qIsDefaultAspect(QtVideo::Rotation rotation)
{
    return qIsDefaultAspect(qToUnderlying(rotation));
}
}

QuickVideoOutput::QuickVideoOutput(QQuickItem *parent) :
    QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setAcceptedMouseButtons(Qt::NoButton);
    setAcceptHoverEvents(false);

    m_sink = new QuickVideoSink(this);
    qRegisterMetaType<VideoFrameFormat>();

    connect(m_sink, &VideoSink::videoFrameChanged, this,
            makeGuardedCall([this](const VideoFrame &frame) {
        if (frame.isValid() || m_endOfStreamPolicy == ClearOutput)
            setFrame(frame);
    }),
            Qt::DirectConnection);

    connect(m_sink->platformVideoSink(), &PlatformVideoSink::subtitleChanged, this,
            makeGuardedCall([this]() {
        {
            QMutexLocker lock(&m_frameMutex);
            m_subtitleChanged = true;
        }
        QMetaObject::invokeMethod(this, [this]() { update(); });
    }),
            Qt::DirectConnection);

    connect(m_sink->platformVideoSink(), &PlatformVideoSink::subtitleStyleChanged, this,
            makeGuardedCall([this]() {
        {
            QMutexLocker lock(&m_frameMutex);
            m_subtitleStyleChanged = true;
            m_subtitleChanged = true;
        }
        QMetaObject::invokeMethod(this, [this]() { update(); });
    }),
            Qt::DirectConnection);

    initRhiForSink();
}

QuickVideoOutput::~QuickVideoOutput()
{
    {
        QMutexLocker lock(&m_destructorGuard->m_mutex);
        m_destructorGuard->m_isAlive = false;
    }

    delete m_sink;
    disconnectWindowConnections();
}

VideoSink *QuickVideoOutput::videoSink() const
{
    return m_sink;
}

QuickVideoOutput::FillMode QuickVideoOutput::fillMode() const
{
    return FillMode(m_aspectRatioMode);
}

void QuickVideoOutput::setFillMode(FillMode mode)
{
    if (mode == fillMode())
        return;

    m_aspectRatioMode = Qt::AspectRatioMode(mode);

    m_geometryDirty = true;
    update();

    emit fillModeChanged(mode);
}

void QuickVideoOutput::_q_newFrame(QSize size)
{
    update();

    size = qRotatedFrameSize(size, m_frameDisplayingRotation);

    if (m_nativeSize != size) {
        m_nativeSize = size;

        m_geometryDirty = true;

        setImplicitWidth(size.width());
        setImplicitHeight(size.height());

        emit sourceRectChanged();
    }
}

void QuickVideoOutput::_q_updateGeometry()
{
    const QRectF rect(0, 0, width(), height());
    const QRectF absoluteRect(x(), y(), width(), height());

    if (!m_geometryDirty && m_lastRect == absoluteRect)
        return;

    QRectF oldContentRect(m_contentRect);

    m_geometryDirty = false;
    m_lastRect = absoluteRect;

    const auto fill = m_aspectRatioMode;
    if (m_nativeSize.isEmpty()) {

        m_contentRect = rect;
    } else if (fill == Qt::IgnoreAspectRatio) {
        m_contentRect = rect;
    } else {
        QSizeF scaled = m_nativeSize;
        scaled.scale(rect.size(), fill);

        m_contentRect = QRectF(QPointF(), scaled);
        m_contentRect.moveCenter(rect.center());
    }

    updateGeometry();

    if (m_contentRect != oldContentRect)
        emit contentRectChanged();
}

int QuickVideoOutput::orientation() const
{
    return m_orientation;
}

void QuickVideoOutput::setOrientation(int orientation)
{

    if (orientation % 90)
        return;

    if (m_orientation == orientation)
        return;

    if (qVideoRotationFromDegrees(orientation - m_orientation) == QtVideo::Rotation::None) {
        m_orientation = orientation;
        emit orientationChanged();
        return;
    }

    m_geometryDirty = true;

    bool oldAspect = qIsDefaultAspect(m_orientation);
    bool newAspect = qIsDefaultAspect(orientation);

    m_orientation = orientation;

    {
        QMutexLocker lock(&m_frameMutex);
        m_frameDisplayingRotation = qNormalizedFrameTransformation(m_frame, m_orientation).rotation;
    }

    if (oldAspect != newAspect) {
        m_nativeSize.transpose();

        setImplicitWidth(m_nativeSize.width());
        setImplicitHeight(m_nativeSize.height());

    }

    update();
    emit orientationChanged();
}

bool QuickVideoOutput::mirrored() const
{
    return m_mirrored;
}

void QuickVideoOutput::setMirrored(bool mirrored)
{
    if (m_mirrored == mirrored)
        return;
    m_mirrored = mirrored;

    update();
    emit mirroredChanged();
}

qreal QuickVideoOutput::radius() const
{
    return m_radius;
}

void QuickVideoOutput::setRadius(qreal radius)
{
    if (qFuzzyCompare(m_radius, radius))
        return;
    m_radius = radius;

    update();
    emit radiusChanged();
}

QRectF QuickVideoOutput::contentRect() const
{
    return m_contentRect;
}

QuickVideoOutput::EndOfStreamPolicy QuickVideoOutput::endOfStreamPolicy() const
{
    return m_endOfStreamPolicy;
}

void QuickVideoOutput::setEndOfStreamPolicy(EndOfStreamPolicy policy)
{
    if (m_endOfStreamPolicy == policy)
        return;

    m_endOfStreamPolicy = policy;
    emit endOfStreamPolicyChanged(policy);
}

void QuickVideoOutput::clearOutput()
{
    setFrame({});
}

QRectF QuickVideoOutput::sourceRect() const
{

    QSizeF size = m_nativeSize;
    if (!size.isValid())
        return {};

    if (!qIsDefaultAspect(m_frameDisplayingRotation))
        size.transpose();

    const QRectF viewport = adjustedViewport();
    Q_ASSERT(viewport.size() == size);
    return QRectF(viewport.topLeft(), size);
}

void QuickVideoOutput::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    Q_UNUSED(newGeometry);
    Q_UNUSED(oldGeometry);

    QQuickItem::geometryChange(newGeometry, oldGeometry);

    _q_updateGeometry();
}

void QuickVideoOutput::releaseResources()
{

    initRhiForSink();
    QQuickItem::releaseResources();
}

void QuickVideoOutput::initRhiForSink()
{
    QRhi *rhi = m_window ? QQuickWindowPrivate::get(m_window)->rhi : nullptr;
    m_sink->setRhi(rhi);
}

void QuickVideoOutput::itemChange(QQuickItem::ItemChange change,
                                    const QQuickItem::ItemChangeData &changeData)
{
    if (change != QQuickItem::ItemSceneChange)
        return;

    if (changeData.window == m_window)
        return;

    disconnectWindowConnections();
    m_window = changeData.window;

    if (m_window) {
        auto connectToWindow = [&](auto signal, auto function) {
            connect(m_window, signal, this, makeGuardedCall(std::move(function)),
                    Qt::DirectConnection);
        };

        connectToWindow(&QQuickWindow::sceneGraphInitialized, [this] {
            initRhiForSink();
        });

        connectToWindow(&QQuickWindow::sceneGraphInvalidated, [this] {
            if (auto texturePool = m_texturePool.lock())
                texturePool->clearTextures();
            m_sink->setRhi(nullptr);
        });

        connectToWindow(&QQuickWindow::afterFrameEnd, [this] {
            if (auto texturePool = m_texturePool.lock())
                texturePool->onFrameEndInvoked();
        });
    }
    initRhiForSink();
}

QSize QuickVideoOutput::nativeSize() const
{
    return m_videoFormat.viewport().size();
}

void QuickVideoOutput::updateGeometry()
{
    const QRectF viewport = m_videoFormat.viewport();
    const QSizeF frameSize = m_videoFormat.frameSize();
    const QRectF normalizedViewport(viewport.x() / frameSize.width(),
                                    viewport.y() / frameSize.height(),
                                    viewport.width() / frameSize.width(),
                                    viewport.height() / frameSize.height());
    const QRectF rect(0, 0, width(), height());
    if (nativeSize().isEmpty()) {
        m_renderedRect = rect;
        m_sourceTextureRect = normalizedViewport;
    } else if (m_aspectRatioMode == Qt::IgnoreAspectRatio) {
        m_renderedRect = rect;
        m_sourceTextureRect = normalizedViewport;
    } else if (m_aspectRatioMode == Qt::KeepAspectRatio) {
        m_sourceTextureRect = normalizedViewport;
        m_renderedRect = contentRect();
    } else if (m_aspectRatioMode == Qt::KeepAspectRatioByExpanding) {
        m_renderedRect = rect;
        const qreal contentHeight = contentRect().height();
        const qreal contentWidth = contentRect().width();

        const qreal relativeOffsetLeft = -contentRect().left() / contentWidth;
        const qreal relativeOffsetTop = -contentRect().top() / contentHeight;
        const qreal relativeWidth = rect.width() / contentWidth;
        const qreal relativeHeight = rect.height() / contentHeight;

        const qreal totalOffsetLeft = normalizedViewport.x() + relativeOffsetLeft * normalizedViewport.width();
        const qreal totalOffsetTop = normalizedViewport.y() + relativeOffsetTop * normalizedViewport.height();
        const qreal totalWidth = normalizedViewport.width() * relativeWidth;
        const qreal totalHeight = normalizedViewport.height() * relativeHeight;

        if (qIsDefaultAspect(m_frameDisplayingRotation)) {
            m_sourceTextureRect = QRectF(totalOffsetLeft, totalOffsetTop,
                                         totalWidth, totalHeight);
        } else {
            m_sourceTextureRect = QRectF(totalOffsetTop, totalOffsetLeft,
                                         totalHeight, totalWidth);
        }
    }
}

QSGNode *QuickVideoOutput::updatePaintNode(QSGNode *oldNode,
                                             QQuickItem::UpdatePaintNodeData *data)
{
    Q_UNUSED(data);
    _q_updateGeometry();

    QSGVideoNode *videoNode = static_cast<QSGVideoNode *>(oldNode);

    QMutexLocker lock(&m_frameMutex);

    if (m_frameChanged) {
        if (videoNode && videoNode->pixelFormat() != m_frame.pixelFormat()) {
            qz::Log::cat_debug(qLcVideo, "updatePaintNode: deleting old video node because frame format changed");
            delete videoNode;
            videoNode = nullptr;
        }

        if (!m_frame.isValid()) {
            qz::Log::cat_debug(qLcVideo, "updatePaintNode: no frames yet");
            m_frameChanged = false;
            return nullptr;
        }

        if (!videoNode) {

            updateGeometry();
            QRhi *rhi = m_window ? QQuickWindowPrivate::get(m_window)->rhi : nullptr;
            videoNode = new QSGVideoNode(this, m_videoFormat, rhi);
            m_texturePool = videoNode->texturePool();
            qz::Log::cat_debug(qLcVideo, "updatePaintNode: Video node created. Handle type: {}", static_cast<int>(m_frame.handleType()));
        }
    }

    if (!videoNode) {
        m_frameChanged = false;
        m_frame = VideoFrame();
        return nullptr;
    }

    if (m_frameChanged) {
        videoNode->setCurrentFrame(m_frame);

        updateHdr(videoNode);

        m_frameChanged = false;
        if (!m_subtitleStyleChanged)
            m_subtitleChanged = false;
        m_frame = VideoFrame();
    }

    if ((m_subtitleChanged || m_subtitleStyleChanged) && videoNode) {
        m_subtitleChanged = false;
        m_subtitleStyleChanged = false;
        VideoFrame currentFrame = m_sink->videoFrame();
        auto style = m_sink->platformVideoSink()->subtitleStyle();
        if (currentFrame.isValid())
            videoNode->updateSubtitle(currentFrame, style);
    }

    videoNode->setTexturedRectGeometry(
            m_renderedRect, m_sourceTextureRect,
            VideoTransformation{ qVideoRotationFromDegrees(orientation()), m_mirrored });

    videoNode->setRoundedRectParams(static_cast<float>(m_radius), m_renderedRect);

    return videoNode;
}

void QuickVideoOutput::updateHdr(QSGVideoNode *videoNode)
{
    auto *videoOutputWindow = window();
    if (!videoOutputWindow)
        return;

    auto *swapChain = videoOutputWindow->swapChain();
    if (!swapChain)
        return;

    auto requiredSwapChainFormat =
            m_sink->hdrPolicy() == PlaybackOptions::HdrPolicy::Disabled
                    ? QRhiSwapChain::SDR
                    : qGetRequiredSwapChainFormat(m_frame.surfaceFormat());

#ifdef Q_OS_ANDROID
    // Android 不支持交换链 HDR 模式，强制使用 SDR
    requiredSwapChainFormat = QRhiSwapChain::SDR;
#endif

    if (requiredSwapChainFormat != m_lastSwapChainFormat
        && qShouldUpdateSwapChainFormat(swapChain, requiredSwapChainFormat, m_sink->hdrPolicy())) {
        auto *recreateSwapChainJob = QRunnable::create([swapChain, requiredSwapChainFormat]() {
            swapChain->destroy();
            swapChain->setFormat(requiredSwapChainFormat);
            swapChain->createOrResize();
        });

        videoOutputWindow->scheduleRenderJob(recreateSwapChainJob, QQuickWindow::AfterSwapStage);
        m_lastSwapChainFormat = requiredSwapChainFormat;
    }

    videoNode->setSurfaceFormat(swapChain->format());
    videoNode->setHdrInfo(swapChain->hdrInfo());

    const bool isActiveHdr = swapChain->format() == QRhiSwapChain::HDRExtendedSrgbLinear;
    if (m_sink->activeHdr() != isActiveHdr) {
        QMetaObject::invokeMethod(m_sink, [this, isActiveHdr]() {
            m_sink->setActiveHdr(isActiveHdr);
        }, Qt::QueuedConnection);
    }
}

void QuickVideoOutput::disconnectWindowConnections()
{
    if (!m_window)
        return;

    m_window->disconnect(this);
}

QRectF QuickVideoOutput::adjustedViewport() const
{
    return m_videoFormat.viewport();
}

void QuickVideoOutput::setFrame(const VideoFrame &frame)
{
    {
        QMutexLocker lock(&m_frameMutex);

        m_videoFormat = frame.surfaceFormat();
        m_frame = frame;
        m_frameDisplayingRotation = qNormalizedFrameTransformation(frame, m_orientation).rotation;
        m_frameChanged = true;
    }

    QMetaObject::invokeMethod(this, &QuickVideoOutput::_q_newFrame, frame.size());
}

std::optional<std::chrono::nanoseconds> QuickVideoOutput::g_signalBackoff;
void QuickVideoOutput::setSignalBackoff(std::optional<std::chrono::nanoseconds> ns)
{
    g_signalBackoff = ns;
}

QT_END_NAMESPACE

#include "moc_QuickVideoOutput_p.cpp"
