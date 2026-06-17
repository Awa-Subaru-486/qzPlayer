// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_VIDEO_VIDEOSINK_H
#define QT_VIDEO_VIDEOSINK_H
#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/PlaybackOptions.h>
#include <qzMultimedia/VideoFrame.h>
#include <QtCore/qobject.h>
#include <QtGui/qwindowdefs.h>
#include <SubtitleStyle.h>

class QRectF;
class VideoFrameFormat;
class VideoFrame;

class VideoSinkPrivate;
class PlatformVideoSink;
class QRhi;

// 视频输出：接收视频帧并渲染到窗口或纹理
class QZ_MULTIMEDIA_EXPORT VideoSink : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString subtitleText READ subtitleText WRITE setSubtitleText NOTIFY subtitleTextChanged)
    Q_PROPERTY(SubtitleStyle subtitleStyle READ subtitleStyle WRITE setSubtitleStyle NOTIFY subtitleStyleChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(PlaybackOptions::HdrPolicy hdrPolicy READ hdrPolicy WRITE setHdrPolicy NOTIFY hdrPolicyChanged)
    Q_PROPERTY(bool activeHdr READ activeHdr NOTIFY activeHdrChanged)
public:
    explicit VideoSink(QObject *parent = nullptr);
    ~VideoSink() override;

    // RHI 渲染上下文
    [[nodiscard]] QRhi *rhi() const;
    void setRhi(QRhi *rhi);

    // 视频尺寸
    [[nodiscard]] QSize videoSize() const;

    // 字幕文本
    [[nodiscard]] QString subtitleText() const;
    void setSubtitleText(const QString &subtitle);

    // 字幕样式
    [[nodiscard]] SubtitleStyle subtitleStyle() const;
    void setSubtitleStyle(const SubtitleStyle &style);

    // HDR 策略
    [[nodiscard]] PlaybackOptions::HdrPolicy hdrPolicy() const;
    void setHdrPolicy(PlaybackOptions::HdrPolicy policy);

    // 实际 HDR 状态
    [[nodiscard]] bool activeHdr() const;
    void setActiveHdr(bool active);

    // 字幕位图(PGS等图形字幕)
    [[nodiscard]] QImage subtitleImage() const;
    void setSubtitleImage(const QImage &image, const QRect &rect);
    [[nodiscard]] QRect subtitleRect() const;

    // 字幕原始调色板索引位图(GPU端调色板查找渲染)
    void setSubtitleBitmapData(const SubtitleBitmapData &data);
    [[nodiscard]] SubtitleBitmapData subtitleBitmapData() const;

    // 视频帧
    void setVideoFrame(const VideoFrame &frame);
    [[nodiscard]] VideoFrame videoFrame() const;

    // 平台视频输出
    [[nodiscard]] PlatformVideoSink *platformVideoSink() const;
Q_SIGNALS:
    void videoFrameChanged(const VideoFrame &frame) QT6_ONLY(const);
    void subtitleTextChanged(const QString &subtitleText) QT6_ONLY(const);
    void subtitleStyleChanged();
    void videoSizeChanged();
    void hdrPolicyChanged();
    void activeHdrChanged(bool active);

private:
    friend class MediaPlayerPrivate;
    friend class MediaCaptureSessionPrivate;
    void setSource(QObject *source);

    VideoSinkPrivate *d = nullptr;
};

#endif
