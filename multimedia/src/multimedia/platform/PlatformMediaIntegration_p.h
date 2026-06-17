// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PLATFORM_PLATFORMMEDIAINTEGRATION_P_H
#define QT_PLATFORM_PLATFORMMEDIAINTEGRATION_P_H

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <expected>

#include <qzMultimedia/private/MultimediaUtils_p.h>
#include <qzMultimedia/private/MultimediaGlobal_p.h>
#include <qzMultimedia/private/PlatformPreviewFrameProvider_p.h>

#include <memory>
#include <mutex>

class AudioFormat;
class AudioInput;
class AudioOutput;
class MediaDevices;
class MediaPlayer;
class PlatformAudioDevices;
class PlatformAudioInput;
class PlatformAudioOutput;
class PlatformAudioResampler;
class PlatformMediaFormatInfo;
class PlatformMediaPlayer;
class PlatformVideoSink;
class VideoFrame;
class VideoSink;

class QZ_MULTIMEDIA_EXPORT AbstractPlatformSpecificInterface
{
public:
    virtual ~AbstractPlatformSpecificInterface() = default;
};

// 平台媒体集成入口：工厂 + 单例，负责创建所有平台组件
class QZ_MULTIMEDIA_EXPORT PlatformMediaIntegration : public QObject
{
    Q_OBJECT
public:
    // 获取全局单例
    static PlatformMediaIntegration *instance();

    explicit PlatformMediaIntegration(QLatin1String);
    ~PlatformMediaIntegration() override;
    // 获取支持的媒体格式信息
    const PlatformMediaFormatInfo *formatInfo();

    // 创建音频重采样器
    virtual std::expected<std::unique_ptr<PlatformAudioResampler>, QString>
    createAudioResampler(const AudioFormat & ,
                         const AudioFormat & );
    // 创建播放器
    virtual std::expected<PlatformMediaPlayer *, QString> createPlayer(MediaPlayer *)
    {
        return std::unexpected{ notAvailable };
    }

    // 创建音频输入
    virtual std::expected<PlatformAudioInput *, QString> createAudioInput(AudioInput *);
    // 创建音频输出
    virtual std::expected<PlatformAudioOutput *, QString> createAudioOutput(AudioOutput *);

    // 创建视频输出
    virtual std::expected<PlatformVideoSink *, QString> createVideoSink(VideoSink *)
    {
        return std::unexpected{ notAvailable };
    }

    // 创建预览帧提供者
    virtual PlatformPreviewFrameProvider *createPreviewFrameProvider()
    {
        return nullptr;
    }

    // 获取音频设备管理器
    PlatformAudioDevices *audioDevices();

    // 获取可用后端列表
    static QStringList availableBackends();
    // 获取当前后端名称
    QLatin1String name();

    // 视频帧格式转换
    virtual VideoFrame convertVideoFrame(VideoFrame &, const VideoFrameFormat &);

    // 获取平台特定接口
    virtual AbstractPlatformSpecificInterface *platformSpecificInterface();

    // 获取音频后端名称
    static QLatin1String audioBackendName();

protected:
    virtual PlatformMediaFormatInfo *createFormatInfo();

    virtual std::unique_ptr<PlatformAudioDevices> createAudioDevices();

    inline static const QString notAvailable = QStringLiteral("Not available");

private:
    friend class MockIntegration;
    void resetInstance();

private:
    mutable std::unique_ptr<PlatformMediaFormatInfo> m_formatInfo;
    mutable std::once_flag m_formatInfoOnceFlg;

    std::unique_ptr<PlatformAudioDevices> m_audioDevices;
    std::once_flag m_audioDevicesOnceFlag;

    std::unique_ptr<AbstractPlatformSpecificInterface> m_platformSpecificInterface;

    const QLatin1String m_backendName;
};

#endif
