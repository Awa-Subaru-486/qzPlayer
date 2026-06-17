// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLAYBACK_PLAYBACKOPTIONS_H
#define QT_PLAYBACK_PLAYBACKOPTIONS_H
#include <qzMultimedia/qtmultimediaexports.h>
#include <qzMultimedia/MultimediaGlobal.h>
#include <QtCore/qcompare.h>
#include <QtCore/qobject.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qvector.h>

#include <chrono>

class PlaybackOptionsPrivate;
QT_DECLARE_QESDP_SPECIALIZATION_DTOR(PlaybackOptionsPrivate)

// 播放选项配置类：控制网络超时、解码策略、零拷贝等播放行为
class PlaybackOptions
{
    Q_GADGET_EXPORT(QZ_MULTIMEDIA_EXPORT)
    Q_PROPERTY(std::chrono::milliseconds networkTimeout READ networkTimeout WRITE setNetworkTimeout RESET
                       resetNetworkTimeout FINAL)
    Q_PROPERTY(PlaybackIntent playbackIntent READ playbackIntent WRITE setPlaybackIntent RESET
                       resetPlaybackIntent)
    Q_PROPERTY(qsizetype probeSize READ probeSize WRITE setProbeSize RESET resetProbeSize)
    Q_PROPERTY(HdrPolicy hdrPolicy READ hdrPolicy WRITE setHdrPolicy RESET resetHdrPolicy)
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
public:
    // 播放意图：普通播放或低延迟流媒体
    enum class PlaybackIntent {
        Playback,
        LowLatencyStreaming,
    };
    Q_ENUM(PlaybackIntent)

    // 视频解码策略：软件或硬件
    enum class VideoDecoderPolicy {
        Software,
        HardwareD3D11VA,
        HardwareVkVideo,
        HardwareMediaVideo,
    };
    Q_ENUM(VideoDecoderPolicy)

    // 零拷贝模式
    enum class ZeroCopy {
        Disabled,
        Enabled,
    };
    Q_ENUM(ZeroCopy)

    // HDR 策略
    enum class HdrPolicy {
        Disabled,
        Enabled,
    };
    Q_ENUM(HdrPolicy)

    QZ_MULTIMEDIA_EXPORT PlaybackOptions();
    QZ_MULTIMEDIA_EXPORT PlaybackOptions(const PlaybackOptions &);
    QZ_MULTIMEDIA_EXPORT PlaybackOptions &operator=(const PlaybackOptions &);
    PlaybackOptions(PlaybackOptions &&) noexcept = default;
    QT_MOVE_ASSIGNMENT_OPERATOR_IMPL_VIA_PURE_SWAP(PlaybackOptions)
    QZ_MULTIMEDIA_EXPORT ~PlaybackOptions();

    void swap(PlaybackOptions &other) noexcept { d.swap(other.d); }

    // 网络超时设置
    [[nodiscard]] QZ_MULTIMEDIA_EXPORT std::chrono::milliseconds networkTimeout() const;
    QZ_MULTIMEDIA_EXPORT void setNetworkTimeout(std::chrono::milliseconds timeout);
    QZ_MULTIMEDIA_EXPORT void resetNetworkTimeout();

    // 播放意图设置
    [[nodiscard]] QZ_MULTIMEDIA_EXPORT PlaybackIntent playbackIntent() const;
    QZ_MULTIMEDIA_EXPORT void setPlaybackIntent(PlaybackIntent intent);
    QZ_MULTIMEDIA_EXPORT void resetPlaybackIntent();

    // 探测大小设置
    [[nodiscard]] QZ_MULTIMEDIA_EXPORT qsizetype probeSize() const;
    QZ_MULTIMEDIA_EXPORT void setProbeSize(qsizetype probeSizeBytes);
    QZ_MULTIMEDIA_EXPORT void resetProbeSize();

    // 视频解码器优先级
    [[nodiscard]] QZ_MULTIMEDIA_EXPORT QVector<VideoDecoderPolicy> videoDecoderPriority() const;
    QZ_MULTIMEDIA_EXPORT void setVideoDecoderPriority(const QVector<VideoDecoderPolicy> &priority);
    // 优先使用指定解码器
    QZ_MULTIMEDIA_EXPORT void prioritizeDecoder(VideoDecoderPolicy policy);
    QZ_MULTIMEDIA_EXPORT void deprioritizeDecoder(VideoDecoderPolicy policy);

    // 零拷贝设置
    [[nodiscard]] QZ_MULTIMEDIA_EXPORT ZeroCopy zeroCopy() const;
    QZ_MULTIMEDIA_EXPORT void setZeroCopy(ZeroCopy zeroCopy);
    QZ_MULTIMEDIA_EXPORT void resetZeroCopy();

    // HDR 策略设置
    [[nodiscard]] QZ_MULTIMEDIA_EXPORT HdrPolicy hdrPolicy() const;
    QZ_MULTIMEDIA_EXPORT void setHdrPolicy(HdrPolicy policy);
    QZ_MULTIMEDIA_EXPORT void resetHdrPolicy();

private:
    friend QZ_MULTIMEDIA_EXPORT bool comparesEqual(const PlaybackOptions &lhs,
                                                  const PlaybackOptions &rhs) noexcept;
    friend QZ_MULTIMEDIA_EXPORT Qt::strong_ordering compareThreeWay(const PlaybackOptions &lhs,
                                                                   const PlaybackOptions &rhs) noexcept;
    Q_DECLARE_STRONGLY_ORDERED(PlaybackOptions)

    friend class PlaybackOptionsPrivate;
    QExplicitlySharedDataPointer<PlaybackOptionsPrivate> d;
};

Q_DECLARE_SHARED(PlaybackOptions)

#endif
