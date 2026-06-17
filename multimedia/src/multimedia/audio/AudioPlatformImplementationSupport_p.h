// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOPLATFORMIMPLEMENTATIONSUPPORT_P_H
#define QT_AUDIO_AUDIOPLATFORMIMPLEMENTATIONSUPPORT_P_H

#include <qzMultimedia/MultimediaGlobal.h>

#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioSystemPlatformStreamSupport_p.h>

namespace QtMultimediaPrivate {

#ifdef __cpp_concepts

template <typename T>
concept PlatformSinkStream = requires(T t, QIODevice *device,
                                       PlatformAudioIOStream::ShutdownPolicy shutdownPolicy,
                                       AudioSinkCallback callback)
{
    requires std::constructible_from<
        T,
        AudioDevice,
        const AudioFormat&,
        std::optional<qsizetype>,
        typename T::SinkType*,
        float,
        std::optional<int32_t>,
        AudioEndpointRole
    >;

    { t.open() } -> std::same_as<bool>;

    { t.start() } -> std::same_as<QIODevice *>;
    { t.start(device) } -> std::same_as<bool>;
    { t.start(std::move(callback)) } -> std::same_as<bool>;

    { t.suspend() } -> std::same_as<void>;
    { t.resume() } -> std::same_as<void>;
    { t.stop(shutdownPolicy) } -> std::same_as<void>;

    { t.setVolume(0.0f) } -> std::same_as<void>;
    { t.bytesFree() } -> std::same_as<quint64>;

    { t.processedDuration() } -> std::same_as<std::chrono::microseconds>;
};

#  define STREAM_TYPE_ARG PlatformSinkStream StreamType
#else
#  define STREAM_TYPE_ARG typename StreamType
#endif

template <STREAM_TYPE_ARG, typename DerivedType = void>
class PlatformAudioSinkImplementation : public PlatformAudioSink
{
public:
    PlatformAudioSinkImplementation(AudioDevice device, const AudioFormat &format,
                                     QObject *parent);
    ~PlatformAudioSinkImplementation() override;

    void start(QIODevice *device) override;
    void start(AudioCallback &&) override;
    QIODevice *start() override;

    void stop() override final;
    void reset() override;

    void suspend() override;
    void resume() override;

    qsizetype bytesFree() const override;
    void setBufferSize(qsizetype value) override;
    qsizetype bufferSize() const override;
    void setHardwareBufferFrames(int32_t) override;
    int32_t hardwareBufferFrames() override;
    qint64 processedUSecs() const override;

    void setVolume(float volume) override;
    bool hasCallbackAPI() override;
    void setRole(AudioEndpointRole role) override;

protected:
    void handleStreamOpenError();

    friend class QtMultimediaPrivate::PlatformAudioSinkStream;
    friend StreamType;
    using ShutdownPolicy = PlatformAudioIOStream::ShutdownPolicy;

    std::optional<int> m_internalBufferSize;
    std::optional<int32_t> m_hardwareBufferFrames;
    AudioEndpointRole m_role = AudioEndpointRole::Other;

    std::shared_ptr<StreamType> m_stream;

    using ConcreteSinkType = std::conditional_t<std::is_same_v<DerivedType, void>,
                                                PlatformAudioSinkImplementation, DerivedType>;
};

template <STREAM_TYPE_ARG, typename DerivedType>
PlatformAudioSinkImplementation<StreamType, DerivedType>::PlatformAudioSinkImplementation(
        AudioDevice device, const AudioFormat &format, QObject *parent)
    : PlatformAudioSink(std::move(device), format, parent)
{
}

template <STREAM_TYPE_ARG, typename DerivedType>
PlatformAudioSinkImplementation<StreamType, DerivedType>::~PlatformAudioSinkImplementation()
{
    stop();
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::start(QIODevice *device)
{
    if (!device) {
        setError(Audio::IOError);
        return;
    }

    if (m_stream) {
        qWarning("AudioSink::start() called while already started");
        return;
    }

    m_stream = std::make_shared<StreamType>(m_audioDevice, m_format, m_internalBufferSize,
                                            static_cast<ConcreteSinkType *>(this), volume(),
                                            m_hardwareBufferFrames, m_role);

    if (!m_stream->open())
        return handleStreamOpenError();

    bool started = m_stream->start(device);
    if (!started)
        return handleStreamOpenError();

    updateStreamState(Audio::ActiveState);
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::handleStreamOpenError()
{
    m_stream->requestStop();
    setError(Audio::OpenError);
    m_stream = {};
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::start(AudioCallback &&audioCallback)
{
    using namespace QtMultimediaPrivate;
    if (!validateAudioCallback(audioCallback, m_format)) {
        setError(Audio::OpenError);
        return;
    }

    if (m_stream) {
        qWarning("AudioSink::start() called while already started");
        return;
    }

    m_stream = std::make_shared<StreamType>(m_audioDevice, m_format, m_internalBufferSize,
                                            static_cast<ConcreteSinkType *>(this), volume(),
                                            m_hardwareBufferFrames, m_role);

    if (!m_stream->open())
        return handleStreamOpenError();

    bool started = m_stream->start(std::move(audioCallback));
    if (!started)
        return handleStreamOpenError();

    updateStreamState(Audio::ActiveState);
}

template <STREAM_TYPE_ARG, typename DerivedType>
QIODevice *PlatformAudioSinkImplementation<StreamType, DerivedType>::start()
{
    if (m_stream) {
        qWarning("AudioSink::start() called while already started");
        return nullptr;
    }

    m_stream = std::make_shared<StreamType>(m_audioDevice, m_format, m_internalBufferSize,
                                            static_cast<ConcreteSinkType *>(this), volume(),
                                            m_hardwareBufferFrames, m_role);

    if (!m_stream->open()) {
        handleStreamOpenError();
        return nullptr;
    }

    QIODevice *device = m_stream->start();
    if (!device) {
        handleStreamOpenError();
        return nullptr;
    }

    QObject::connect(device, &QIODevice::readyRead, this, [this] {
        updateStreamIdle(false);
    });
    updateStreamIdle(true);
    updateStreamState(Audio::ActiveState);
    return device;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::stop()
{
    if (m_stream) {
        m_stream->stop(ShutdownPolicy::DrainRingbuffer);
        m_stream = {};
        updateStreamState(Audio::StoppedState);
    }
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::reset()
{
    if (m_stream) {
        m_stream->stop(ShutdownPolicy::DiscardRingbuffer);
        m_stream = {};
        updateStreamState(Audio::StoppedState);
    }
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::suspend()
{
    if (m_stream) {
        m_stream->suspend();
        updateStreamState(Audio::SuspendedState);
    }
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::resume()
{
    if (m_stream) {
        updateStreamState(Audio::ActiveState);
        m_stream->resume();
    }
}

template <STREAM_TYPE_ARG, typename DerivedType>
qsizetype PlatformAudioSinkImplementation<StreamType, DerivedType>::bytesFree() const
{
    if (m_stream)
        return m_stream->bytesFree();
    return 0;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::setBufferSize(qsizetype value)
{
    m_internalBufferSize = value;
}

template <STREAM_TYPE_ARG, typename DerivedType>
qsizetype PlatformAudioSinkImplementation<StreamType, DerivedType>::bufferSize() const
{
    if (m_stream)
        return m_stream->ringbufferSizeInBytes();

    return PlatformAudioIOStream::inferRingbufferBytes(m_internalBufferSize,
                                                        m_hardwareBufferFrames, m_format);
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::setHardwareBufferFrames(int32_t arg)
{
    if (arg > 0)
        m_hardwareBufferFrames = arg;
    else
        m_hardwareBufferFrames = std::nullopt;
}

template <STREAM_TYPE_ARG, typename DerivedType>
int32_t PlatformAudioSinkImplementation<StreamType, DerivedType>::hardwareBufferFrames()
{
    return m_hardwareBufferFrames.value_or(-1);
}

template <STREAM_TYPE_ARG, typename DerivedType>
qint64 PlatformAudioSinkImplementation<StreamType, DerivedType>::processedUSecs() const
{
    if (m_stream)
        return m_stream->processedDuration().count();

    return 0;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::setVolume(float volume)
{
    PlatformAudioEndpointBase::setVolume(volume);
    if (m_stream)
        m_stream->setVolume(volume);
}

template <STREAM_TYPE_ARG, typename DerivedType>
bool PlatformAudioSinkImplementation<StreamType, DerivedType>::hasCallbackAPI()
{
    return true;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSinkImplementation<StreamType, DerivedType>::setRole(AudioEndpointRole role)
{
    m_role = role;
}

#undef STREAM_TYPE_ARG

#ifdef __cpp_concepts

template <typename T>
concept PlatformSourceStream = requires(T t, QIODevice *device,
                                         PlatformAudioIOStream::ShutdownPolicy shutdownPolicy)
{
    requires std::constructible_from<
        T,
        AudioDevice,
        const AudioFormat&,
        std::optional<qsizetype>,
        typename T::SourceType*,
        float,
        std::optional<int32_t>
    >;

    { t.open() } -> std::same_as<bool>;

    { t.start() } -> std::same_as<QIODevice *>;
    { t.start(device) } -> std::same_as<bool>;

    { t.suspend() } -> std::same_as<void>;
    { t.resume() } -> std::same_as<void>;
    { t.stop(shutdownPolicy) } -> std::same_as<void>;

    { t.setVolume(0.0f) } -> std::same_as<void>;
    { t.bytesReady() } -> std::same_as<qsizetype>;
    { t.deviceIsRingbufferReader() } -> std::same_as<bool>;

    { t.processedDuration() } -> std::same_as<std::chrono::microseconds>;
};

#  define STREAM_TYPE_ARG PlatformSourceStream StreamType
#else
#  define STREAM_TYPE_ARG typename StreamType
#endif

template <STREAM_TYPE_ARG, typename DerivedType = void>
class PlatformAudioSourceImplementation : public PlatformAudioSource
{
public:
    PlatformAudioSourceImplementation(AudioDevice device, const AudioFormat &format,
                                       QObject *parent);
    ~PlatformAudioSourceImplementation() override;

    void start(QIODevice *device) override;
    QIODevice *start() override;

    void stop() override final;
    void reset() override;

    void suspend() override;
    void resume() override;

    qsizetype bytesReady() const override;
    void setBufferSize(qsizetype value) override;
    qsizetype bufferSize() const override;
    void setHardwareBufferFrames(int32_t) override;
    int32_t hardwareBufferFrames() override;
    qint64 processedUSecs() const override;

    void setVolume(float volume) override;

protected:
    void handleStreamOpenError();

    friend class QtMultimediaPrivate::PlatformAudioSourceStream;
    friend StreamType;
    using ShutdownPolicy = PlatformAudioIOStream::ShutdownPolicy;

    std::optional<int> m_internalBufferSize;
    std::optional<int32_t> m_hardwareBufferFrames;

    std::shared_ptr<StreamType> m_stream;
    std::shared_ptr<StreamType> m_retiredStream;

    using ConcreteSourceType = std::conditional_t<std::is_same_v<DerivedType, void>,
                                                  PlatformAudioSourceImplementation, DerivedType>;
};

template <STREAM_TYPE_ARG, typename DerivedType>
PlatformAudioSourceImplementation<StreamType, DerivedType>::PlatformAudioSourceImplementation(
        AudioDevice device, const AudioFormat &format, QObject *parent)
    : PlatformAudioSource(std::move(device), format, parent)
{
}

template <STREAM_TYPE_ARG, typename DerivedType>
PlatformAudioSourceImplementation<StreamType, DerivedType>::~PlatformAudioSourceImplementation()
{
    stop();
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::handleStreamOpenError()
{
    m_stream->requestStop();
    setError(Audio::OpenError);
    m_stream = {};
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::start(QIODevice *device)
{
    if (!device) {
        setError(Audio::IOError);
        return;
    }

    if (m_stream) {
        qWarning("AudioSource::start() called while already started");
        return;
    }

    m_stream = std::make_shared<StreamType>(m_audioDevice, m_format, m_internalBufferSize,
                                            static_cast<ConcreteSourceType *>(this), volume(),
                                            m_hardwareBufferFrames);

    if (!m_stream->open())
        return handleStreamOpenError();

    bool started = m_stream->start(device);
    if (!started)
        return handleStreamOpenError();

    updateStreamState(Audio::ActiveState);
}

template <STREAM_TYPE_ARG, typename DerivedType>
QIODevice *PlatformAudioSourceImplementation<StreamType, DerivedType>::start()
{
    if (m_stream) {
        qWarning("AudioSource::start() called while already started");
        return nullptr;
    }

    m_stream = std::make_shared<StreamType>(m_audioDevice, m_format, m_internalBufferSize,
                                            static_cast<ConcreteSourceType *>(this), volume(),
                                            m_hardwareBufferFrames);

    if (!m_stream->open()) {
        handleStreamOpenError();
        return nullptr;
    }

    QIODevice *device = m_stream->start();
    if (!device) {
        handleStreamOpenError();
        return nullptr;
    }

    QObject::connect(device, &QIODevice::readyRead, this, [this] {
        updateStreamIdle(false);
    });
    updateStreamIdle(true, EmitStateSignal::False);
    updateStreamState(Audio::ActiveState);
    return device;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::stop()
{
    if (!m_stream)
        return;

    if (m_stream->deviceIsRingbufferReader())

        m_retiredStream = m_stream;

    m_stream->stop(ShutdownPolicy::DrainRingbuffer);
    m_stream = {};
    updateStreamState(Audio::StoppedState);
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::reset()
{
    m_retiredStream = {};

    if (!m_stream)
        return;

    m_stream->stop(ShutdownPolicy::DiscardRingbuffer);
    m_stream = {};
    updateStreamState(Audio::StoppedState);
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::suspend()
{
    if (m_stream) {
        m_stream->suspend();
        updateStreamState(Audio::SuspendedState);
    }
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::resume()
{
    if (m_stream) {
        updateStreamState(Audio::ActiveState);
        m_stream->resume();
    }
}

template <STREAM_TYPE_ARG, typename DerivedType>
qsizetype PlatformAudioSourceImplementation<StreamType, DerivedType>::bytesReady() const
{
    return m_stream ? m_stream->bytesReady() : 0;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::setBufferSize(qsizetype value)
{
    m_internalBufferSize = value;
}

template <STREAM_TYPE_ARG, typename DerivedType>
qsizetype PlatformAudioSourceImplementation<StreamType, DerivedType>::bufferSize() const
{
    if (m_stream)
        return m_stream->ringbufferSizeInBytes();

    return PlatformAudioIOStream::inferRingbufferBytes(m_internalBufferSize,
                                                        m_hardwareBufferFrames, m_format);
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::setHardwareBufferFrames(
        int32_t arg)
{
    if (arg > 0)
        m_hardwareBufferFrames = arg;
    else
        m_hardwareBufferFrames = std::nullopt;
}

template <STREAM_TYPE_ARG, typename DerivedType>
int32_t PlatformAudioSourceImplementation<StreamType, DerivedType>::hardwareBufferFrames()
{
    return m_hardwareBufferFrames.value_or(-1);
}

template <STREAM_TYPE_ARG, typename DerivedType>
qint64 PlatformAudioSourceImplementation<StreamType, DerivedType>::processedUSecs() const
{
    return m_stream ? m_stream->processedDuration().count() : 0;
}

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementation<StreamType, DerivedType>::setVolume(float volume)
{
    PlatformAudioEndpointBase::setVolume(volume);
    if (m_stream)
        m_stream->setVolume(volume);
}

template <STREAM_TYPE_ARG, typename DerivedType = void>
class PlatformAudioSourceImplementationWithCallback
    : public PlatformAudioSourceImplementation<StreamType, DerivedType>
{
protected:
    using BaseClass = PlatformAudioSourceImplementation<StreamType, DerivedType>;
    using BaseClass::BaseClass;
    using BaseClass::start;

    QT_WARNING_PUSH
    QT_WARNING_DISABLE_CLANG("-Woverloaded-virtual")
    void start(PlatformAudioSource::AudioCallback &&) override;
    QT_WARNING_POP
    bool hasCallbackAPI() override { return true; }
};

template <STREAM_TYPE_ARG, typename DerivedType>
void PlatformAudioSourceImplementationWithCallback<StreamType, DerivedType>::start(
        PlatformAudioSource::AudioCallback &&audioCallback)
{
    using namespace QtMultimediaPrivate;
    if (!validateAudioCallback(audioCallback, BaseClass::m_format)) {
        BaseClass::setError(Audio::OpenError);
        return;
    }

    BaseClass::m_stream = std::make_shared<StreamType>(
            BaseClass::m_audioDevice, BaseClass::m_format, BaseClass::m_internalBufferSize,
            static_cast<typename BaseClass::ConcreteSourceType *>(this), BaseClass::volume(),
            BaseClass::m_hardwareBufferFrames);

    if (!BaseClass::m_stream->open())
        return BaseClass::handleStreamOpenError();

    bool started = BaseClass::m_stream->start(std::move(audioCallback));
    if (!started)
        return BaseClass::handleStreamOpenError();

    BaseClass::updateStreamState(Audio::ActiveState);
}

#undef STREAM_TYPE_ARG

}

#endif
