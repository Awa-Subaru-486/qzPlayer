// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "RtAudioEngine_p.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qdebug.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>

#include <qzMultimedia/private/AudioRtsanSupport_p.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/MemoryResourceTlsf_p.h>

#include <QtCore/q20map.h>
#include <mutex>

#ifdef Q_CC_MINGW

QT_WARNING_PUSH
QT_WARNING_DISABLE_GCC("-Wmaybe-uninitialized")
#endif

namespace QtMultimediaPrivate {

using namespace QtPrivate;
using namespace std::chrono_literals;

namespace {
struct AudioDeviceFormatLess
{
    bool operator()(const std::pair<AudioDevice, AudioFormat> &lhs,
                    const std::pair<AudioDevice, AudioFormat> &rhs) const
    {
        auto cmp = qCompareThreeWay(lhs.first.id(), rhs.first.id());
        if (cmp == Qt::strong_ordering::less)
            return true;
        if (cmp == Qt::strong_ordering::greater)
            return false;

        return std::tuple(lhs.second.sampleRate(), lhs.second.sampleFormat(),
                          lhs.second.channelCount())
                < std::tuple(rhs.second.sampleRate(), rhs.second.sampleFormat(),
                             rhs.second.channelCount());
    }
};
}

std::shared_ptr<RtAudioEngine>
RtAudioEngine::getEngineFor(const AudioDevice &device, const AudioFormat &format)
{
    if (device.isNull()) {
        qWarning() << "RtAudioEngine needs to be called with a valid device";
        return nullptr;
    }

    if (format.sampleFormat() != AudioFormat::Float) {
        qWarning() << "RtAudioEngine requires floating point samples";
        return nullptr;
    }

    if (!device.isFormatSupported(format)) {
        qWarning() << "RtAudioEngine needs to be called with a supported fromat";
        return nullptr;
    }

    static QMutex s_playerRegistryMutex;
    static std::map<std::pair<AudioDevice, AudioFormat>, std::weak_ptr<RtAudioEngine>,
                    AudioDeviceFormatLess>
            s_playerRegistry;

    auto guard = std::lock_guard{ s_playerRegistryMutex };

    auto key = std::pair{ device, format };
    auto found = s_playerRegistry.find(key);
    if (found != s_playerRegistry.end()) {
        auto player = found->second.lock();
        if (player)
            return player;
    }

    q20::erase_if(s_playerRegistry, [](auto &&keyValuePair) {
        return keyValuePair.second.expired();
    });

    auto player = std::shared_ptr<RtAudioEngine>(new RtAudioEngine{ device, format },
                                                  [](RtAudioEngine *engine) {
        engine->deleteLater();
    });
    s_playerRegistry.emplace(key, player);

    return player;
}

RtAudioEngine::RtAudioEngine(const AudioDevice &device, const AudioFormat &format)
    : m_sink{
          device,
          format,
      },
      m_rtMemoryPool {
           std::make_unique<TlsfMemoryResource>(poolSize)
      }
{
    m_notificationEvent.callOnActivated([this] {
        runNonRtNotifications();
    });

    if (!QThread::isMainThread()) {
        QThread *appThread = qApp->thread();
        moveToThread(appThread);
        m_sink.moveToThread(appThread);
        m_notificationEvent.moveToThread(appThread);
        m_pendingCommandsTimer.moveToThread(appThread);
    }

    m_pendingCommandsTimer.setInterval(10ms);
    m_pendingCommandsTimer.setTimerType(Qt::CoarseTimer);
    m_pendingCommandsTimer.callOnTimeout(&m_pendingCommandsTimer, [this] {
        auto lock = std::lock_guard{ m_mutex };
        sendPendingRtCommands();
        if (m_appToRtOverflowBuffer.empty())
            m_pendingCommandsTimer.stop();
    });

    PlatformAudioSink *platformSink = PlatformAudioSink::get(m_sink);

    platformSink->setRole(QtMultimediaPrivate::AudioEndpointRole::SoundEffect);

    platformSink->start([this](std::span<float> outputBuffer) {
        audioCallback(outputBuffer);
    });

    platformSink->suspend();
}

RtAudioEngine::~RtAudioEngine()
{
    m_sink.reset();

    m_appToRt.consumeAll([](auto) {
    });
    m_rtToApp.consumeAll([](auto) {
    });
}

void RtAudioEngine::play(SharedVoice voice)
{
    auto lock = std::lock_guard{ m_mutex };

    Q_ASSERT(voice->format() == m_sink.format());

    if (m_voices.empty())
        m_sink.resume();

    m_voices.insert(voice);

    sendAppToRtCommand(PlayCommand{
            std::move(voice),
    });
}

void RtAudioEngine::stop(const SharedVoice &voice)
{
    stop(voice->voiceId());
}

void RtAudioEngine::stop(VoiceId voiceId)
{
    auto lock = std::lock_guard{ m_mutex };
    sendAppToRtCommand(StopCommand{ voiceId });
}

void RtAudioEngine::visitVoiceRt(VoiceId voiceId, RtVoiceVisitor fn, bool visitorIsTrivial)
{
    auto lock = std::lock_guard{ m_mutex };

    if (visitorIsTrivial) {
        sendAppToRtCommand(VisitCommandTrivial{
                voiceId,
                std::move(fn),
        });

    } else {
        sendAppToRtCommand(VisitCommand{
                voiceId,
                std::move(fn),
        });
    }
}

VoiceId RtAudioEngine::allocateVoiceId()
{
    static std::atomic_uint64_t allocator{ 0 };
    return VoiceId{ allocator.fetch_add(1, std::memory_order_relaxed) };
}

void RtAudioEngine::audioCallback(std::span<float> outputBuffer) noexcept QT_MM_NONBLOCKING
{
    runRtCommands();
    bool sendNotification = sendPendingAppNotifications();

    std::fill(outputBuffer.begin(), outputBuffer.end(), 0.f);

    std::vector<SharedVoice, pmr::polymorphic_allocator<SharedVoice>> finishedVoices{
        m_rtMemoryPool.get(),
    };

    for (const SharedVoice &voice : m_rtVoiceRegistry) {
        Q_ASSERT(voice.use_count() >= 2);

        VoicePlayResult playResult = voice->play(outputBuffer);
        if (playResult == VoicePlayResult::Finished)
            finishedVoices.push_back(voice);
    }

    if (!finishedVoices.empty()) {
        withRTSanDisabled([&] {
            for (const SharedVoice &voice : finishedVoices) {
                m_rtVoiceRegistry.erase(voice);
                bool stopSent = sendRtToAppNotification(StopNotification{ voice });
                if (stopSent)
                    sendNotification = true;
            }
        });
    }

    cleanupRetiredVoices();
    if (sendNotification)
        m_notificationEvent.set();
}

void RtAudioEngine::cleanupRetiredVoices() noexcept QT_MM_NONBLOCKING
{
    bool notifyApp = false;

#if __cpp_lib_erase_if >= 202002L
    using std::erase_if;
#else
    auto erase_if = [](auto &c, auto &&pred) {
        auto old_size = c.size();
        for (auto first = c.begin(), last = c.end(); first != last;) {
            if (pred(*first))
                first = c.erase(first);
            else
                ++first;
        }
        return old_size - c.size();
    };
#endif
    withRTSanDisabled([&] {
        erase_if(m_rtVoiceRegistry, [&](const SharedVoice &voice) {
            bool voiceIsActive = voice->isActive();
            if (!voiceIsActive)
                notifyApp = sendRtToAppNotification(StopNotification{ voice });

            return !voiceIsActive;
        });
    });

    if (notifyApp)
        m_notificationEvent.set();
}

void RtAudioEngine::runRtCommands() noexcept QT_MM_NONBLOCKING
{
    m_appToRt.consumeAll([&](std::span<RtCommand> commands) {
        for (RtCommand &cmd : commands) {
            std::visit([&](auto cmd) {
                runRtCommand(std::move(cmd));
            }, std::move(cmd));
        }
    });
}

void RtAudioEngine::runRtCommand(PlayCommand cmd) noexcept QT_MM_NONBLOCKING
{
    withRTSanDisabled([&] {
        m_rtVoiceRegistry.insert(cmd.voice);
    });
}

void RtAudioEngine::runRtCommand(StopCommand cmd) noexcept QT_MM_NONBLOCKING
{
    auto it = m_rtVoiceRegistry.find(cmd.voiceId);
    if (it == m_rtVoiceRegistry.end())
        return;

    SharedVoice voice = *it;
    m_rtVoiceRegistry.erase(it);

    bool emitNotify = sendRtToAppNotification(StopNotification{
            std::move(voice),
    });
    if (emitNotify)
        m_notificationEvent.set();
}

void RtAudioEngine::runRtCommand(VisitCommand cmd) noexcept QT_MM_NONBLOCKING
{
    auto it = m_rtVoiceRegistry.find(cmd.voiceId);
    if (it == m_rtVoiceRegistry.end())
        return;

    cmd.callback(**it);

    bool emitNotify = sendRtToAppNotification(VisitReply{
            std::move(cmd.callback),
    });
    if (emitNotify)
        m_notificationEvent.set();
}

void RtAudioEngine::runRtCommand(VisitCommandTrivial cmd) noexcept QT_MM_NONBLOCKING
{
    auto it = m_rtVoiceRegistry.find(cmd.voiceId);
    if (it == m_rtVoiceRegistry.end())
        return;

    cmd.callback(**it);
}

void RtAudioEngine::runNonRtNotifications()
{
    std::vector<VoiceId> finishedVoices;
    {
        auto lock = std::lock_guard{ m_mutex };
        m_rtToApp.consumeAll([&](std::span<Notification> notifications) {
            for (Notification &notification : notifications) {
                std::visit([&](auto notification) {
                    runNonRtNotification(std::move(notification));
                }, std::move(notification));
            }
        });

        finishedVoices = std::move(m_finishedVoices);
        m_finishedVoices.clear();
    }

    for (VoiceId voiceId : finishedVoices)
        emit voiceFinished(voiceId);
}

void RtAudioEngine::runNonRtNotification(StopNotification notification)
{
    m_voices.erase(notification.voice);
    if (m_voices.empty())
        m_sink.suspend();
    m_finishedVoices.push_back(notification.voice->voiceId());
}

void RtAudioEngine::runNonRtNotification(VisitReply)
{

}

void RtAudioEngine::sendAppToRtCommand(RtCommand cmd)
{

    sendPendingRtCommands();

    bool written = m_appToRt.produceOne([&] {
        return std::move(cmd);
    });

    if (written)
        return;

    m_appToRtOverflowBuffer.emplace_back(std::move(cmd));

    QMetaObject::invokeMethod(&m_pendingCommandsTimer, [this] {
        if (!m_pendingCommandsTimer.isActive())
            m_pendingCommandsTimer.start();
    });
}

bool RtAudioEngine::sendRtToAppNotification(Notification cmd)
{

    bool emitNotification = sendPendingAppNotifications();

    bool written = m_rtToApp.produceOne([&] {
        return std::move(cmd);
    });

    if (written)
        return true;

    m_rtToAppOverflowBuffer.emplace_back(std::move(cmd));

    return emitNotification;
}

void RtAudioEngine::sendPendingRtCommands()
{
    while (!m_appToRtOverflowBuffer.empty()) {
        Q_UNLIKELY_BRANCH;
        bool written = m_appToRt.produceOne([&] {
            return std::move(m_appToRtOverflowBuffer.front());
        });
        if (!written)
            return;

        m_appToRtOverflowBuffer.pop_front();
    }
}

bool RtAudioEngine::sendPendingAppNotifications()
{
    bool emitNotification = false;
    while (!m_rtToAppOverflowBuffer.empty()) {
        Q_UNLIKELY_BRANCH;

        bool written = m_rtToApp.produceOne([&] {
            return std::move(m_rtToAppOverflowBuffer.front());
        });
        if (!written)
            break;

        m_rtToAppOverflowBuffer.pop_front();
        emitNotification = true;
    }

    return emitNotification;
}

}

#ifdef Q_CC_MINGW
QT_WARNING_POP
#endif

#include "moc_RtAudioEngine_p.cpp"
