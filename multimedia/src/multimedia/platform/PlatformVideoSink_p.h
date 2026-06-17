// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMVIDEOSINK_P_H
#define QT_PLATFORM_PLATFORMVIDEOSINK_P_H

#include <qzMultimedia/MultimediaGlobal.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qmutex.h>
#include <QtGui/qwindowdefs.h>
#include <VideoSink.h>
#include <VideoFrame.h>
#include <SubtitleStyle.h>
#include <qdebug.h>
#include <private/qglobal_p.h>

class QString;

// 平台视频输出抽象接口：负责视频帧渲染到窗口
class QZ_MULTIMEDIA_EXPORT PlatformVideoSink : public QObject
{
    Q_OBJECT

public:
    ~PlatformVideoSink() override;

    // 设置 RHI 渲染上下文
    virtual void setRhi(QRhi * ) {}

    // 设置窗口 ID
    virtual void setWinId(WId) {}
    // 设置显示区域
    virtual void setDisplayRect(const QRect &) {};
    // 设置全屏模式
    virtual void setFullScreen(bool) {}
    // 设置宽高比模式
    virtual void setAspectRatioMode(Qt::AspectRatioMode) {}

    // 获取视频原始尺寸
    QSize nativeSize() const;

    // 图像调整
    virtual void setBrightness(float ) {}
    virtual void setContrast(float ) {}
    virtual void setHue(float ) {}
    virtual void setSaturation(float ) {}

    VideoSink *videoSink() { return m_sink; }

    void setNativeSize(QSize s);

    // 设置/获取当前视频帧
    void setVideoFrame(const VideoFrame &frame);

    VideoFrame currentVideoFrame() const;

    // 字幕文本
    void setSubtitleText(const QString &subtitleText);

    QString subtitleText() const;

    // 字幕位图(PGS等图形字幕)
    void setSubtitleImage(const QImage &image, const QRect &rect);

    QImage subtitleImage() const;
    QRect subtitleRect() const;

    // 字幕原始调色板索引位图(GPU端调色板查找渲染)
    void setSubtitleBitmapData(const SubtitleBitmapData &data);
    SubtitleBitmapData subtitleBitmapData() const;

    // 字幕样式
    void setSubtitleStyle(const SubtitleStyle &style);
    SubtitleStyle subtitleStyle() const;

Q_SIGNALS:
    void subtitleChanged();
    void subtitleStyleChanged();

protected:
    explicit PlatformVideoSink(VideoSink *parent);

    virtual void onVideoFrameChanged(const VideoFrame &) { }

Q_SIGNALS:
    void rhiChanged();

private:
    VideoSink *const m_sink = nullptr;
    mutable QMutex m_mutex;
    QSize m_nativeSize;
    QString m_subtitleText;
    QImage m_subtitleImage;
    QRect m_subtitleRect;
    SubtitleBitmapData m_subtitleBitmapData;
    SubtitleStyle m_subtitleStyle;
    VideoFrame m_currentVideoFrame;
};

#endif
