// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioSink.h"

#include <qzMultimedia/Audio.h>
#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioHelpers_p.h>
#include <qzMultimedia/private/PlatformAudioDevices_p.h>
#include <qzMultimedia/private/PlatformMediaIntegration_p.h>

AudioSink::AudioSink(const AudioFormat &format, QObject *parent)
    : AudioSink({}, format, parent)
{
}

AudioSink::AudioSink(const AudioDevice &audioDevice, const AudioFormat &format, QObject *parent):
    QObject(parent)
{
    d = PlatformMediaIntegration::instance()->audioDevices()->audioOutputDevice(format,
                                                                                 audioDevice, this);
    if (d)
        connect(d, &PlatformAudioSink::stateChanged, this, &AudioSink::stateChanged);
    else
        qWarning("No audio device detected");
}

AudioSink::~AudioSink()
{
    delete d;
}

AudioFormat AudioSink::format() const
{
    return d ? d->format() : AudioFormat();
}

static bool validateFormatAtStart(PlatformAudioSink *d)
{
    if (!d->format().isValid()) {
        qWarning() << "AudioSink::start: AudioFormat not valid";
        d->setError(Audio::OpenError);
        return false;
    }

    if (!d->isFormatSupported(d->format())) {
        qWarning() << "AudioSink::start: AudioFormat not supported by AudioDevice";
        d->setError(Audio::OpenError);
        return false;
    }
    return true;
};

void AudioSink::start(QIODevice* device)
{
    if (!d)
        return;

    d->setError(Audio::NoError);

    if (!device->isReadable()) {
        qWarning() << "AudioSink::start: QIODevice is not readable";
        d->setError(Audio::OpenError);
        return;
    }

    if (!validateFormatAtStart(d))
        return;

    d->elapsedTime.start();
    d->start(device);
}

QIODevice* AudioSink::start()
{
    if (!d)
        return nullptr;

    d->setError(Audio::NoError);

    if (!validateFormatAtStart(d))
        return nullptr;

    d->elapsedTime.start();
    return d->start();
}

void AudioSink::stop()
{
    if (d)
        d->stop();
}

void AudioSink::reset()
{
    if (d)
        d->reset();
}

void AudioSink::suspend()
{
    if (d)
        d->suspend();
}

void AudioSink::resume()
{
    if (d)
        d->resume();
}

qsizetype AudioSink::bytesFree() const
{
    return d ? d->bytesFree() : 0;
}

qsizetype AudioSink::framesFree() const
{
    return d ? d->format().framesForBytes(bytesFree()) : 0;
}

void AudioSink::setBufferSize(qsizetype value)
{
    if (d)
        d->setBufferSize(value);
}

qsizetype AudioSink::bufferSize() const
{
    return d ? d->bufferSize() : 0;
}

void AudioSink::setBufferFrameCount(qsizetype value)
{
    if (d)
        setBufferSize(d->format().bytesForFrames(value));
}

qsizetype AudioSink::bufferFrameCount() const
{
    return d ? d->format().framesForBytes(bufferSize()) : 0;
}

qint64 AudioSink::processedUSecs() const
{
    return d ? d->processedUSecs() : 0;
}

qint64 AudioSink::elapsedUSecs() const
{
    return state() == Audio::StoppedState ? 0 : d->elapsedTime.nsecsElapsed()/1000;
}

QtAudio::Error AudioSink::error() const
{
    return d ? d->error() : Audio::OpenError;
}

QtAudio::State AudioSink::state() const
{
    return d ? d->state() : Audio::StoppedState;
}

void AudioSink::setVolume(qreal volume)
{
    if (!d)
        return;

    std::optional<float> newVolume = AudioHelperInternal::sanitizeVolume(volume, this->volume());
    if (newVolume)
        d->setVolume(*newVolume);
}

qreal AudioSink::volume() const
{
    return d ? d->volume() : 1.0;
}

#include "moc_AudioSink.cpp"
