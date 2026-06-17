// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_WINDOWS_WINDOWSAUDIOSINK_P_H
#define QT_WINDOWS_WINDOWSAUDIOSINK_P_H
#include <QtCore/qthread.h>
#include <QtCore/qtclasshelpermacros.h>
#include <QtCore/private/qcomptr_p.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioSystemPlatformStreamSupport_p.h>
#include <qzMultimedia/private/AudioPlatformImplementationSupport_p.h>
#include <qzMultimedia/private/WindowsAudioUtils_p.h>

#include <atomic>
#include <memory>
#include <memory_resource>

struct IAudioRenderClient;
struct IMMDeviceEnumerator;
struct IMMNotificationClient;

class WindowsResampler;

namespace QtWASAPI {

class WindowsAudioSink;
using namespace QtMultimediaPrivate;

// WASAPI 音频输出流：基于 WASAPI 的音频播放实现
struct WASAPIAudioSinkStream final : std::enable_shared_from_this<WASAPIAudioSinkStream>,
                                      PlatformAudioSinkStream
{
    using SampleFormat = AudioFormat::SampleFormat;
    using SinkType = WindowsAudioSink;

    // 流类型：环形缓冲或回调
    enum class StreamType : uint8_t {
        Ringbuffer,
        Callback,
    };

    WASAPIAudioSinkStream(AudioDevice, const AudioFormat &,
                           std::optional<qsizetype> ringbufferSize, WindowsAudioSink *parent,
                           float volume, std::optional<int32_t> hardwareBufferSize,
                           AudioEndpointRole);
    Q_DISABLE_COPY_MOVE(WASAPIAudioSinkStream)
    ~WASAPIAudioSinkStream();

    bool open();

    using PlatformAudioSinkStream::bytesFree;
    using PlatformAudioSinkStream::processedDuration;
    using PlatformAudioSinkStream::ringbufferSizeInBytes;
    using PlatformAudioSinkStream::setVolume;

    // 启动流
    bool start(QIODevice *);
    QIODevice *start();
    bool start(AudioCallback);

    // 暂停/恢复/停止
    void suspend();
    void resume();
    void stop(ShutdownPolicy);

    void updateStreamIdle(bool) override;

private:
    bool openAudioClient(ComPtr<IMMDevice>, AudioEndpointRole);
    bool startAudioClient(StreamType);

    template <typename Functor>
    bool visitAudioClientBuffer(Functor &&f);

    void fillInitialHostBuffer();
    void runProcessRingbufferLoop();
    void runProcessCallbackLoop();
    bool processRingbuffer() noexcept QT_MM_NONBLOCKING;
    bool processCallback() noexcept QT_MM_NONBLOCKING;

    void handleAudioClientError();
    void joinWorkerThread();

    void setupDeviceMonitor();
    void checkDeviceChange();
    bool reopenStreamWithNewDevice();

    ComPtr<IAudioClient3> m_audioClient;
    ComPtr<IAudioRenderClient> m_renderClient;

    WindowsAudioUtils::reference_time m_periodSize;
    qsizetype m_audioClientFrames;

    std::atomic_bool m_suspended{};
    std::atomic<ShutdownPolicy> m_shutdownPolicy{ ShutdownPolicy::DiscardRingbuffer };
    QAutoResetEvent m_ringbufferDrained;

    const AudioEndpointRole m_role;

    const QUniqueWin32NullHandle m_wasapiHandle;
    std::unique_ptr<QThread> m_workerThread;

    AudioCallback m_audioCallback;

    WindowsAudioSink *m_parent;

    AudioFormat m_hostFormat;
    std::unique_ptr<char[]> m_preallocatedBuffer;
    std::unique_ptr<std::pmr::memory_resource> m_memoryResource;
    std::unique_ptr<WindowsResampler> m_resampler;

    IMMDeviceEnumerator *m_deviceEnumerator = nullptr;
    IMMNotificationClient *m_notificationClient = nullptr;
    std::atomic<bool> m_deviceChanged{false};
    QByteArray m_currentDeviceId;
};

// Windows 音频输出：WASAPI 音频输出实现
class WindowsAudioSink final
    : public PlatformAudioSinkImplementation<WASAPIAudioSinkStream, WindowsAudioSink>
{
    using BaseClass = PlatformAudioSinkImplementation<WASAPIAudioSinkStream, WindowsAudioSink>;

public:
    WindowsAudioSink(AudioDevice, const AudioFormat &, QObject *parent);

    QByteArray currentDeviceId() const { return m_currentDeviceId; }
    void setCurrentDeviceId(const QByteArray &id) { m_currentDeviceId = id; }

private:
    QByteArray m_currentDeviceId;
};

}

#endif
