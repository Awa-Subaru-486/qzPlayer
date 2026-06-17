// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_PORTAUDIO_PORTAUDIOSINK_P_H
#define QT_PORTAUDIO_PORTAUDIOSINK_P_H

#include <qzMultimedia/private/AudioSystemPlatformStreamSupport_p.h>
#include <qzMultimedia/private/AudioPlatformImplementationSupport_p.h>
#include <qzMultimedia/private/AudioHelpers_p.h>
#include <QtCore/qmutex.h>
#include <QtCore/qwaitcondition.h>

#include <portaudio.h>

#include <atomic>
#include <memory>
#include <thread>

#ifdef Q_OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <timeapi.h>
#endif

class PortAudioSink;

class PortAudioSinkStream final : public std::enable_shared_from_this<PortAudioSinkStream>,
                                   public QtMultimediaPrivate::PlatformAudioSinkStream
{
public:
    using SampleFormat = AudioFormat::SampleFormat;
    using SinkType = PortAudioSink;
    using BaseClass = QtMultimediaPrivate::PlatformAudioSinkStream;

    PortAudioSinkStream(AudioDevice device, const AudioFormat &format,
                         std::optional<qsizetype> ringbufferSize, PortAudioSink *parent,
                         float volume, std::optional<int32_t> hardwareBufferSize,
                         QtMultimediaPrivate::AudioEndpointRole role);
    Q_DISABLE_COPY_MOVE(PortAudioSinkStream)
    ~PortAudioSinkStream();

    bool open();

    using BaseClass::bytesFree;
    using BaseClass::processedDuration;
    using BaseClass::ringbufferSizeInBytes;
    using BaseClass::setVolume;

    bool start(QIODevice *device);
    QIODevice *start();
    bool start(AudioCallback callback);

    void suspend();
    void resume();
    void stop(ShutdownPolicy shutdownPolicy);

    void updateStreamIdle(bool streamIsIdle) override;

private:
    static int paCallback(const void *input, void *output, unsigned long frameCount,
                          const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData);

    bool openStream();
    void closeStream();
    bool reopenStreamWithNewDevice();
    void joinWorkerThread();

#ifdef Q_OS_WINDOWS
    void setupDeviceMonitor();
    void checkDeviceChange();
    void increaseTimerResolution();
    void restoreTimerResolution();
#endif

    PortAudioSink *m_parent = nullptr;
    PaStream *m_paStream = nullptr;
    int m_deviceIndex = -1;
    std::optional<AudioHelperInternal::NativeSampleFormat> m_nativeFormat;

    std::atomic_bool m_suspended{ false };
    std::atomic<ShutdownPolicy> m_shutdownPolicy{ ShutdownPolicy::DiscardRingbuffer };

    std::unique_ptr<std::thread> m_workerThread;
    QMutex m_mutex;
    QWaitCondition m_condition;

#ifdef Q_OS_WINDOWS
    IMMDeviceEnumerator *m_deviceEnumerator = nullptr;
    IMMNotificationClient *m_notificationClient = nullptr;
    HANDLE m_workerWakeEvent = nullptr;
    bool m_timerResolutionIncreased = false;
#endif
};

class PortAudioSink final
    : public QtMultimediaPrivate::PlatformAudioSinkImplementation<PortAudioSinkStream, PortAudioSink>
{
    using BaseClass = QtMultimediaPrivate::PlatformAudioSinkImplementation<PortAudioSinkStream, PortAudioSink>;

public:
    PortAudioSink(AudioDevice device, const AudioFormat &format, QObject *parent);
};

#endif
