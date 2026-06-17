// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

/**
 * @file QuickPreviewFrame_p.h
 * @brief 预览帧显示组件，使用自定义 QSGMaterial + YUV/RGB 着色器渲染
 * @details 支持 YUV420P/NV12/RGBA 格式，圆角处理，PreserveAspectCrop 填充
 */

#ifndef QUICKPREVIEWFRAME_P_H
#define QUICKPREVIEWFRAME_P_H

#include <QQuickItem>
#include <QImage>
#include <QUrl>
#include <QFutureWatcher>
#include <QMutex>
#include <QTimer>
#include <atomic>

#include "qzmultimediaquickexports.h"
#include <qzMultimedia/private/PlatformPreviewFrameProvider_p.h>

QT_BEGIN_NAMESPACE

class PreviewFrameProvider;

class QZ_MULTIMEDIAQUICK_EXPORT QuickPreviewFrame : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qreal radius READ radius WRITE setRadius NOTIFY radiusChanged)

public:
    explicit QuickPreviewFrame(QQuickItem *parent = nullptr);
    ~QuickPreviewFrame() override;

    QUrl source() const;
    void setSource(const QUrl &source);

    qint64 position() const;
    void setPosition(qint64 position);

    qreal radius() const;
    void setRadius(qreal r);

    // 供渲染节点访问（调用方需持有 frameMutex）
    const PreviewFrameData &frameData() const { return m_frameData; }
    int generation() const { return m_generation.load(std::memory_order_acquire); }
    qreal radiusValue() const { return m_radius; }

    // 保护 m_frameData 的互斥锁
    // onFrameReady 写入 / updatePaintNode 读取 均需持有此锁
    QMutex &frameMutex() const { return m_frameMutex; }

Q_SIGNALS:
    void sourceChanged();
    void positionChanged();
    void radiusChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private Q_SLOTS:
    void onFrameReady();

private:
    void requestFrame();

    QUrl m_source;
    qint64 m_position = 0;
    qreal m_radius = 0.0;

    // m_frameData 通过 m_frameMutex 保护：
    // - GUI 线程 onFrameReady 写入
    // - 渲染线程 updatePaintNode 读取
    // shared_ptr 不增加引用计数（零拷贝），靠互斥锁保证生命周期
    mutable QMutex m_frameMutex;
    PreviewFrameData m_frameData;
    std::atomic<int> m_generation{0};

    PreviewFrameProvider *m_provider = nullptr;
    QFutureWatcher<PreviewFrameData> m_frameWatcher;

    // 切换源后延迟请求帧，避免与播放器状态变化冲突
    QTimer m_sourceDebounceTimer;

    // 标记源刚切换，下一次 setPosition 时跳过请求
    // 避免切换源后立即解析导致崩溃
    bool m_sourceJustChanged = false;
};

QT_END_NAMESPACE

#endif // QUICKPREVIEWFRAME_P_H
