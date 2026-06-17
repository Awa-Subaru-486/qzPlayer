// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOSYSTEM_P_H
#define QT_AUDIO_AUDIOSYSTEM_P_H

#include <qzMultimedia/MultimediaGlobal.h>

#include <qzMultimedia/Audio.h>
#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/AudioFormat.h>
#include <qzMultimedia/private/AudioHelpers_p.h>
#include <qzMultimedia/private/AudioRtsanSupport_p.h>
#include <qzMultimedia/private/MultimediaAssume_p.h>

#include <QtCore/qelapsedtimer.h>
#include <span>
#include <QtCore/private/qglobal_p.h>

#include <array>
#include <functional>
#include <variant>

class QIODevice;
class AudioSink;
class AudioSource;

namespace QtMultimediaPrivate {

enum class AudioEndpointRole : uint8_t
{
    MediaPlayback,
    SoundEffect,
    Accessibility,
    Other,
};

template <typename SampleType>
using AudioSinkCallbackType = std::function<void(std::span<SampleType>)>;

template <typename SampleType>
using AudioSourceCallbackType = std::function<void(std::span<const SampleType>)>;

#if __cpp_lib_move_only_function
template <typename SampleType>
using AudioSinkMoveOnlyCallbackType = std::move_only_function<void(std::span<SampleType>)>;

template <typename SampleType>
using AudioSourceMoveOnlyCallbackType = std::move_only_function<void(std::span<const SampleType>)>;
#endif

template <typename>
struct GetSampleTypeImpl;

template <>
struct GetSampleTypeImpl<float>
{
    using type = float;
    static constexpr AudioFormat::SampleFormat sample_format = AudioFormat::SampleFormat::Float;
};

template <>
struct GetSampleTypeImpl<int32_t>
{
    using type = int32_t;
    static constexpr AudioFormat::SampleFormat sample_format = AudioFormat::SampleFormat::Int32;
};
template <>
struct GetSampleTypeImpl<int16_t>
{
    using type = int16_t;
    static constexpr AudioFormat::SampleFormat sample_format = AudioFormat::SampleFormat::Int16;
};

template <>
struct GetSampleTypeImpl<uint8_t>
{
    using type = uint8_t;
    static constexpr AudioFormat::SampleFormat sample_format = AudioFormat::SampleFormat::UInt8;
};

template <typename T>
struct GetSampleTypeImpl<AudioSinkCallbackType<T>> : GetSampleTypeImpl<T>
{
};

template <typename T>
struct GetSampleTypeImpl<AudioSourceCallbackType<T>> : GetSampleTypeImpl<T>
{
};

#if __cpp_lib_move_only_function

template <typename T>
struct GetSampleTypeImpl<AudioSinkMoveOnlyCallbackType<T>> : GetSampleTypeImpl<T>
{
};

template <typename T>
struct GetSampleTypeImpl<AudioSourceMoveOnlyCallbackType<T>> : GetSampleTypeImpl<T>
{
};

#endif

template <typename SampleTypeOrCallbackType>
using GetSampleType = typename GetSampleTypeImpl<SampleTypeOrCallbackType>::type;

template <typename SampleTypeOrCallbackType>
static constexpr AudioFormat::SampleFormat getSampleFormat()
{
    return GetSampleTypeImpl<SampleTypeOrCallbackType>::sample_format;
}

#if __cpp_lib_move_only_function
using AudioSinkCallback =
        std::variant<AudioSinkMoveOnlyCallbackType<float>, AudioSinkMoveOnlyCallbackType<uint8_t>,
                     AudioSinkMoveOnlyCallbackType<int16_t>,
                     AudioSinkMoveOnlyCallbackType<int32_t>>;
using AudioSourceCallback = std::variant<
        AudioSourceMoveOnlyCallbackType<float>, AudioSourceMoveOnlyCallbackType<uint8_t>,
        AudioSourceMoveOnlyCallbackType<int16_t>, AudioSourceMoveOnlyCallbackType<int32_t>>;
#else
using AudioSinkCallback =
        std::variant<AudioSinkCallbackType<float>, AudioSinkCallbackType<uint8_t>,
                     AudioSinkCallbackType<int16_t>, AudioSinkCallbackType<int32_t>>;
using AudioSourceCallback =
        std::variant<AudioSourceCallbackType<float>, AudioSourceCallbackType<uint8_t>,
                     AudioSourceCallbackType<int16_t>, AudioSourceCallbackType<int32_t>>;

#endif

template <typename AnyAudioCallback>
constexpr bool validateAudioCallbackImpl(const AnyAudioCallback &audioCallback,
                                         const AudioFormat &format)
{
    bool isNonNullFunction = std::visit([](const auto &cb) {
        return bool(cb);
    }, audioCallback);

    if (!isNonNullFunction)
        return false;

    bool hasCorrectFormat = std::visit([&](const auto &cb) {
        return getSampleFormat<std::decay_t<decltype(cb)>>() == format.sampleFormat();
    }, audioCallback);

    return hasCorrectFormat;
}

constexpr bool validateAudioCallback(const AudioSinkCallback &audioCallback,
                                     const AudioFormat &format)
{
    return validateAudioCallbackImpl(audioCallback, format);
}

constexpr bool validateAudioCallback(const AudioSourceCallback &audioCallback,
                                     const AudioFormat &format)
{
    return validateAudioCallbackImpl(audioCallback, format);
}

template <bool IsSink>
inline void
runAudioCallback(std::conditional_t<IsSink, AudioSinkCallback, AudioSourceCallback> &audioCallback,
                 std::span<std::conditional_t<IsSink, std::byte, const std::byte>> hostBuffer,
                 const AudioFormat &format)
{
    Q_ASSERT(!hostBuffer.empty());

    bool callbackIsValid = validateAudioCallback(audioCallback, format);
    QT_MM_ASSUME(callbackIsValid);

    int numberOfSamples = format.framesForBytes(hostBuffer.size()) * format.channelCount();

    std::visit([&](auto &callback) {
        using FunctorType = std::decay_t<decltype(callback)>;
        Q_ASSERT(getSampleFormat<FunctorType>() == format.sampleFormat());

        using SampleType = GetSampleType<FunctorType>;

        bool audioCallbackIsValid = bool(callback);
        QT_MM_ASSUME(audioCallbackIsValid);
        using HostBufferType = std::conditional_t<IsSink, SampleType, const SampleType>;

        auto buffer = std::span<HostBufferType>{
            reinterpret_cast<HostBufferType *>(hostBuffer.data()),
            static_cast<size_t>(numberOfSamples),
        };
        callback(buffer);
    }, audioCallback);
}

inline void runAudioCallback(AudioSinkCallback &audioCallback, std::span<std::byte> hostBuffer,
                             const AudioFormat &format, float volume)
{
    runAudioCallback<true>(audioCallback, hostBuffer, format);
    AudioHelperInternal::applyVolume(volume, format, hostBuffer, hostBuffer);
}

inline void runAudioCallback(AudioSourceCallback &audioCallback, std::span<const std::byte> hostBuffer,
                             const AudioFormat &format, float volume)
{
    if (volume == 1.0f) {
        runAudioCallback<false>(audioCallback, hostBuffer, format);
    } else {

        constexpr qsizetype sizeEstimate = 1024 * 16 * sizeof(float);
        if (hostBuffer.size() <= sizeEstimate) {
            std::array<std::byte, sizeEstimate> stackBuffer;
            std::span<std::byte> stackBufferSpan{
                stackBuffer.data(),
                hostBuffer.size(),
            };

            AudioHelperInternal::applyVolume(volume, format, hostBuffer, stackBufferSpan);
            runAudioCallback<false>(audioCallback, stackBufferSpan, format);
        } else {
            QtPrivate::ScopedRTSanDisabler allowAllocations;

            auto buffer = q20::make_unique_for_overwrite<std::byte[]>(hostBuffer.size());
            auto heapBufferSpan = std::span{
                buffer.get(),
                hostBuffer.size(),
            };
            AudioHelperInternal::applyVolume(volume, format, hostBuffer, heapBufferSpan);
            runAudioCallback<false>(audioCallback, heapBufferSpan, format);
        }
    }
}

inline void runAudioCallback(AudioSourceCallback &audioCallback, std::span<std::byte> hostBuffer,
                             const AudioFormat &format, float volume)
{
    AudioHelperInternal::applyVolume(volume, format, hostBuffer, hostBuffer);
    runAudioCallback<false>(audioCallback, hostBuffer, format);
}

}

class QZ_MULTIMEDIA_EXPORT PlatformAudioEndpointBase : public QObject
{
    Q_OBJECT

public:
    explicit PlatformAudioEndpointBase(AudioDevice, const AudioFormat &, QObject *parent);

    Audio::Error error() const { return m_error; }
    virtual Audio::State state() const { return m_inferredState; }
    virtual void setError(Audio::Error);

    virtual bool isFormatSupported(const AudioFormat &format) const;
    AudioFormat format() const { return m_format; }

    virtual void setVolume(float volume) { m_volume = volume; }
    float volume() const { return m_volume; }

Q_SIGNALS:
    void stateChanged(QtAudio::State);

protected:
    enum class EmitStateSignal : uint8_t
    {
        True,
        False,
    };

    void updateStreamState(Audio::State);
    void updateStreamIdle(bool idle, EmitStateSignal = EmitStateSignal::True);

    const AudioDevice m_audioDevice;
    const AudioFormat m_format;

private:
    void inferState();

    Audio::State m_streamState = Audio::StoppedState;
    Audio::State m_inferredState = Audio::StoppedState;
    Audio::Error m_error{};
    bool m_streamIsIdle = false;

    float m_volume{ 1.f };
};

class QZ_MULTIMEDIA_EXPORT PlatformAudioSink : public PlatformAudioEndpointBase
{
public:
    explicit PlatformAudioSink(AudioDevice, const AudioFormat &, QObject *parent);
    virtual void start(QIODevice *device) = 0;
    virtual QIODevice* start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void suspend() = 0;
    virtual void resume() = 0;
    virtual qsizetype bytesFree() const = 0;
    virtual void setBufferSize(qsizetype value) = 0;
    virtual qsizetype bufferSize() const = 0;
    virtual void setHardwareBufferFrames(int32_t) { }
    virtual int32_t hardwareBufferFrames() { return -1; }
    virtual qint64 processedUSecs() const = 0;

    using AudioCallback = QtMultimediaPrivate::AudioSinkCallback;

    virtual void start(AudioCallback &&) { }
    virtual bool hasCallbackAPI() { return false; }

    QElapsedTimer elapsedTime;

    static PlatformAudioSink *get(const AudioSink &);

    using AudioEndpointRole = QtMultimediaPrivate::AudioEndpointRole;
    virtual void setRole(AudioEndpointRole) { }
};

class QZ_MULTIMEDIA_EXPORT PlatformAudioSource : public PlatformAudioEndpointBase
{
public:
    explicit PlatformAudioSource(AudioDevice, const AudioFormat &, QObject *parent);
    virtual void start(QIODevice *device) = 0;
    virtual QIODevice* start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;
    virtual void suspend()  = 0;
    virtual void resume() = 0;
    virtual qsizetype bytesReady() const = 0;
    virtual void setBufferSize(qsizetype value) = 0;
    virtual void setHardwareBufferFrames(int32_t) { }
    virtual int32_t hardwareBufferFrames() { return -1; }
    virtual qsizetype bufferSize() const = 0;
    virtual qint64 processedUSecs() const = 0;

    using AudioCallback = QtMultimediaPrivate::AudioSourceCallback;

    virtual void start(AudioCallback &&) { }
    virtual bool hasCallbackAPI() { return false; }

    QElapsedTimer elapsedTime;

    static PlatformAudioSource *get(const AudioSource &);
};

namespace QtMultimediaPrivate {
class PlatformAudioSinkStream;
class PlatformAudioSourceStream;
}

#endif
