// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioSystem_p.h"

#include <QtCore/qdebug.h>
#include <qzMultimedia/AudioSink.h>
#include <qzMultimedia/AudioSource.h>
#include <qzMultimedia/private/PlatformAudioDevices_p.h>

PlatformAudioEndpointBase::PlatformAudioEndpointBase(AudioDevice device,
                                                       const AudioFormat &format, QObject *parent)
    : QObject{ parent }, m_audioDevice{ std::move(device) }, m_format{ format }
{
    Q_ASSERT(parent && "PlatformAudioEndpointBase requires the AudioSink/AudioSource as parent");
}

void PlatformAudioEndpointBase::setError(Audio::Error err)
{
    if (err == m_error)
        return;
    m_error = err;
}

bool PlatformAudioEndpointBase::isFormatSupported(const AudioFormat &format) const
{
    return m_audioDevice.isFormatSupported(format);
}

void PlatformAudioEndpointBase::updateStreamState(Audio::State state)
{
    if (m_streamState == state)
        return;

    m_streamState = state;
    inferState();
}

void PlatformAudioEndpointBase::updateStreamIdle(bool idle, EmitStateSignal emitStateSignal)
{
    if (idle == m_streamIsIdle)
        return;
    m_streamIsIdle = idle;

    if (emitStateSignal == EmitStateSignal::True)
        inferState();
}

void PlatformAudioEndpointBase::inferState()
{

    using State = QtAudio::State;

    State oldState = m_inferredState;

    switch (m_streamState) {
    case State::StoppedState:
        m_inferredState = State::StoppedState;
        break;
    case State::SuspendedState:
        m_inferredState = State::SuspendedState;
        break;
    case State::ActiveState:
        m_inferredState = m_streamIsIdle ? State::IdleState : State::ActiveState;
        break;

    case State::IdleState:
        qCritical() << "Users should not be able to set the state to Idle!";
        Q_UNREACHABLE_RETURN();
    }

    if (oldState != m_inferredState)
        emit stateChanged(m_inferredState);
}

PlatformAudioSink::PlatformAudioSink(AudioDevice device, const AudioFormat &format,
                                       QObject *parent)
    : PlatformAudioEndpointBase(std::move(device), format, parent)
{
}

PlatformAudioSink *PlatformAudioSink::get(const AudioSink &sink)
{
    return sink.d;
}

PlatformAudioSource::PlatformAudioSource(AudioDevice device, const AudioFormat &format,
                                           QObject *parent)
    : PlatformAudioEndpointBase(std::move(device), format, parent)
{
}

PlatformAudioSource *PlatformAudioSource::get(const AudioSource &source)
{
    return source.d;
}

#include "moc_AudioSystem_p.cpp"
