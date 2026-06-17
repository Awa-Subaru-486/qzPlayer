// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.


#ifndef QT_WINDOWS_WINDOWSAUDIOSOURCE_P_H
#define QT_WINDOWS_WINDOWSAUDIOSOURCE_P_H
#include <QtCore/qthread.h>
#include <QtCore/qtclasshelpermacros.h>
#include <QtCore/private/qcomptr_p.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioSystemPlatformStreamSupport_p.h>
#include <qzMultimedia/private/AudioPlatformImplementationSupport_p.h>
#include <qzMultimedia/private/WindowsAudioUtils_p.h>
#include <qzMultimedia/private/WindowsResampler_p.h>

#include <atomic>
#include <memory>
#include <memory_resource>

struct IMMDevice;
struct IAudioCaptureClient;
struct IMMDeviceEnumerator;
struct IMMNotificationClient;

namespace QtWASAPI {

class WindowsAudioSource;
using namespace QtMultimediaPrivate;

// WASAPI 音频输入流：基于 WASAPI 的音频采集实现
struct WASAPIAudioSourceStream final : std::enable_shared_from_this<WASAPIAudioSourceStream>,
                                        PlatformAudioSourceStream
{
    using SampleFormat = AudioFormat::SampleFormat;
    using SourceType = WindowsAudioSource;

    WASAPIAudioSourceStream(AudioDevice, const AudioFormat &,
                             std::optional<qsizetype> ringbufferSize, WindowsAudioSource *parent,
                             float volume, std::optional<int32_t> hardwareBufferFrames);
    Q_DISABLE_COPY_MOVE(WASAPIAudioSourceStream)
    ~WASAPIAudioSourceStream();

    using PlatformAudioSourceStream::bytesReady;
    using PlatformAudioSourceStream::deviceIsRingbufferReader;
    using PlatformAudioSourceStream::processedDuration;
    using PlatformAudioSourceStream::ringbufferSizeInBytes;
    using PlatformAudioSourceStream::setVolume;

    bool open() { return true; }
    // 启动流
    bool start(QIODevice *);
    QIODevice *start();
    bool start(AudioCallback &&);

    // 暂停/恢复/停止
    void suspend();
    void resume();
    void stop(ShutdownPolicy);

    void updateStreamIdle(bool) override;

private:
    bool openAudioClient(ComPtr<IMMDevice> device);
    bool startAudioClient();

    template <typename Functor>
    bool visitAudioClientBuffer(Functor &&);

    void runProcessLoop();
    bool processRingbuffer() noexcept QT_MM_NONBLOCKING;
    bool processCallback() noexcept QT_MM_NONBLOCKING;
    void handleAudioClientError();
    void joinWorkerThread();

    void setupDeviceMonitor();
    void checkDeviceChange();
    bool reopenStreamWithNewDevice();

    ComPtr<IAudioClient3> m_audioClient;
    ComPtr<IAudioCaptureClient> m_captureClient;

    WindowsAudioUtils::reference_time m_periodSize;
    qsizetype m_audioClientFrames;

    std::atomic_bool m_suspended{};
    std::atomic<ShutdownPolicy> m_shutdownPolicy{ ShutdownPolicy::DiscardRingbuffer };
    QAutoResetEvent m_ringbufferDrained;

    const QUniqueWin32NullHandle m_wasapiHandle;
    std::unique_ptr<QThread> m_workerThread;

    std::optional<AudioCallback> m_audioCallback;
    WindowsAudioSource *m_parent;

    AudioFormat m_hostFormat;
    std::unique_ptr<std::pmr::memory_resource> m_memoryResource;
    std::unique_ptr<WindowsResampler> m_resampler;

    IMMDeviceEnumerator *m_deviceEnumerator = nullptr;
    IMMNotificationClient *m_notificationClient = nullptr;
    std::atomic<bool> m_deviceChanged{false};
    QByteArray m_currentDeviceId;
};

// Windows 音频输入：WASAPI 音频采集实现
class WindowsAudioSource final
    : public PlatformAudioSourceImplementationWithCallback<WASAPIAudioSourceStream,
                                                            WindowsAudioSource>
{
    using BaseClass = PlatformAudioSourceImplementationWithCallback<WASAPIAudioSourceStream,
                                                                     WindowsAudioSource>;

public:
    WindowsAudioSource(AudioDevice, const AudioFormat &, QObject *parent);

    QByteArray currentDeviceId() const { return m_currentDeviceId; }
    void setCurrentDeviceId(const QByteArray &id) { m_currentDeviceId = id; }

private:
    QByteArray m_currentDeviceId;
};

}

#endif
