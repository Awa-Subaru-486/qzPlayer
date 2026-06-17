// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_RTAUDIOENGINE_P_H
#define QT_AUDIO_RTAUDIOENGINE_P_H

#include <QtCore/qtclasshelpermacros.h>
#include <QtCore/qtimer.h>
#include <QtCore/qmutex.h>

#include <qzMultimedia/AudioSink.h>
#include <qzMultimedia/MultimediaGlobal.h>
#include <qzMultimedia/private/AudioRtsanSupport_p.h>
#include <qzMultimedia/private/AudioRingBuffer_p.h>
#include <qzMultimedia/private/AutoResetEvent_p.h>
#include <qzMultimedia/private/PmrEmulation_p.h>

#include <cstdint>
#include <deque>
#include <set>
#include <variant>
#include <vector>

namespace QtMultimediaPrivate {

enum class VoiceId : uint64_t
{
};

enum class VoicePlayResult : uint8_t
{
    Playing,
    Finished,
};

class QZ_MULTIMEDIA_EXPORT RtAudioEngineVoice
{
public:
    using VoicePlayResult = QtMultimediaPrivate::VoicePlayResult;
    using VoiceId = QtMultimediaPrivate::VoiceId;

    explicit RtAudioEngineVoice(VoiceId id) : m_voiceId{ id } { }
    Q_DISABLE_COPY_MOVE(RtAudioEngineVoice)
    virtual ~RtAudioEngineVoice() = default;

    [[nodiscard]] virtual VoicePlayResult play(std::span<float>) noexcept QT_MM_NONBLOCKING = 0;
    virtual bool isActive() noexcept QT_MM_NONBLOCKING = 0;

    virtual const AudioFormat &format() noexcept = 0;

    VoiceId voiceId() const { return m_voiceId; }

private:
    const VoiceId m_voiceId;
};

struct RtAudioEngineVoiceCompare : std::less<uint64_t>
{
    using std::less<uint64_t>::operator();
    template <typename Lhs, typename Rhs>
    bool operator()(const Lhs &lhs, const Rhs &rhs) const
    {
        return operator()(getId(lhs), getId(rhs));
    }

    static uint64_t getId(VoiceId arg) { return qToUnderlying(arg); }
    static uint64_t getId(const RtAudioEngineVoice &arg) { return getId(arg.voiceId()); }
    static uint64_t getId(const std::shared_ptr<RtAudioEngineVoice> &arg) { return getId(*arg); }

    using is_transparent = std::true_type;
};

namespace Impl {
template <typename T>
struct visitor_arg;

template <typename R, typename Arg>
struct visitor_arg<R(Arg)>
{
    using type = Arg;
};

template <typename R, typename Arg>
struct visitor_arg<R (*)(Arg)>
{
    using type = Arg;
};

template <typename F>
struct visitor_arg : visitor_arg<decltype(&F::operator())>
{
};

template <typename C, typename R, typename Arg>
struct visitor_arg<R (C::*)(Arg) const>
{
    using type = Arg;
};

template <typename C, typename R, typename Arg>
struct visitor_arg<R (C::*)(Arg)>
{
    using type = Arg;
};

}

template <typename F>
using visitor_arg_t = typename Impl::visitor_arg<F>::type;

class QZ_MULTIMEDIA_EXPORT RtAudioEngine final : public QObject
{
public:
    using RtVoiceVisitor = std::function<void(RtAudioEngineVoice &)>;
    using SharedVoice = std::shared_ptr<RtAudioEngineVoice>;

    Q_OBJECT

    struct PlayCommand
    {
        SharedVoice voice;
    };

    struct StopCommand
    {
        const VoiceId voiceId;
    };

    struct VisitCommand
    {
        const VoiceId voiceId;
        RtVoiceVisitor callback;
    };

    struct VisitCommandTrivial
    {
        const VoiceId voiceId;
        RtVoiceVisitor callback;
    };

    using RtCommand = std::variant<PlayCommand, StopCommand, VisitCommand, VisitCommandTrivial>;

    struct StopNotification
    {
        SharedVoice voice;
    };

    struct VisitReply
    {
        RtVoiceVisitor callback;
    };

    using Notification = std::variant<StopNotification, VisitReply>;

public:

    static std::shared_ptr<RtAudioEngine> getEngineFor(const AudioDevice &, const AudioFormat &);

    RtAudioEngine(const AudioDevice &, const AudioFormat &);
    Q_DISABLE_COPY_MOVE(RtAudioEngine)
    ~RtAudioEngine() override;

    void play(SharedVoice);
    void stop(const SharedVoice &);
    void stop(VoiceId);

    template <typename Visitor>
    void visitVoiceRt(VoiceId id, Visitor visitor)
    {
        using visitorArg = visitor_arg_t<Visitor>;
        static_assert(std::is_reference_v<visitorArg>);

        constexpr size_t smallBufferOptimizationEstimate = 2 * sizeof(void *);
        constexpr bool visitorIsTrivial = std::is_trivially_destructible_v<std::decay_t<Visitor>>
                && sizeof(Visitor) <= smallBufferOptimizationEstimate;

        auto wrapped = [visitor = std::move(visitor)](RtAudioEngineVoice &voice) {
            visitor(static_cast<visitorArg>(voice));
        };
        visitVoiceRt(id, RtVoiceVisitor{ wrapped }, visitorIsTrivial);
    }

    template <typename Visitor>
    void visitVoiceRt(const SharedVoice &voice, Visitor visitor)
    {
        visitVoiceRt(voice->voiceId(), std::move(visitor));
    }

    static VoiceId allocateVoiceId();

    std::unique_ptr<pmr::memory_resource> &rtMemoryResource() { return m_rtMemoryPool; }

    AudioSink &audioSink() { return m_sink; }
    const auto &voices() const { return m_voices; }

Q_SIGNALS:
    void voiceFinished(VoiceId);

private:
    void visitVoiceRt(VoiceId, RtVoiceVisitor, bool visitorIsTrivial);

    void audioCallback(std::span<float>) noexcept QT_MM_NONBLOCKING;
    void cleanupRetiredVoices() noexcept QT_MM_NONBLOCKING;

    void runRtCommands() noexcept QT_MM_NONBLOCKING;
    void runRtCommand(PlayCommand) noexcept QT_MM_NONBLOCKING;
    void runRtCommand(StopCommand) noexcept QT_MM_NONBLOCKING;
    void runRtCommand(VisitCommand) noexcept QT_MM_NONBLOCKING;
    void runRtCommand(VisitCommandTrivial) noexcept QT_MM_NONBLOCKING;

    void runNonRtNotifications();
    void runNonRtNotification(StopNotification);
    void runNonRtNotification(VisitReply);

    AudioSink m_sink;

    QMutex m_mutex;

    std::set<SharedVoice, RtAudioEngineVoiceCompare> m_voices;

    static constexpr size_t poolSize = 128 * 1024;
    std::unique_ptr<pmr::memory_resource> m_rtMemoryPool;

    using VoiceRegistry = std::set<SharedVoice, RtAudioEngineVoiceCompare,
                                   pmr::polymorphic_allocator<SharedVoice>>;
    VoiceRegistry m_rtVoiceRegistry{
        m_rtMemoryPool.get(),
    };

    static constexpr size_t commandBuffersSize = 2048;
    QtPrivate::AudioRingBuffer<RtCommand> m_appToRt{ commandBuffersSize };
    QtPrivate::AudioRingBuffer<Notification> m_rtToApp{ commandBuffersSize };
    std::deque<RtCommand> m_appToRtOverflowBuffer;
    std::deque<Notification, pmr::polymorphic_allocator<Notification>> m_rtToAppOverflowBuffer{
        m_rtMemoryPool.get(),
    };
    void sendAppToRtCommand(RtCommand cmd);
    bool sendRtToAppNotification(Notification cmd);
    void sendPendingRtCommands();
    bool sendPendingAppNotifications();
    QTimer m_pendingCommandsTimer;

    QtPrivate::QAutoResetEvent m_notificationEvent;
    std::vector<VoiceId> m_finishedVoices;
};

}

#endif
