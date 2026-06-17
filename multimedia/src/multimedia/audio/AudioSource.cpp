// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioSource.h"

#include <qzMultimedia/Audio.h>
#include <qzMultimedia/AudioDevice.h>
#include <qzMultimedia/private/AudioSystem_p.h>
#include <qzMultimedia/private/AudioHelpers_p.h>
#include <qzMultimedia/private/PlatformAudioDevices_p.h>
#include <qzMultimedia/private/PlatformMediaIntegration_p.h>

AudioSource::AudioSource(const AudioFormat &format, QObject *parent)
    : AudioSource({}, format, parent)
{
}

AudioSource::AudioSource(const AudioDevice &audioDevice, const AudioFormat &format, QObject *parent):
    QObject(parent)
{
    d = PlatformMediaIntegration::instance()->audioDevices()->audioInputDevice(format, audioDevice,
                                                                                this);
    if (d)
        connect(d, &PlatformAudioSource::stateChanged, this, &AudioSource::stateChanged);
    else
        qWarning("No audio device detected");
}

AudioSource::~AudioSource()
{
    delete d;
}

static bool validateFormatAtStart(PlatformAudioSource *d)
{
    if (!d->format().isValid()) {
        qWarning() << "AudioSource::start: AudioFormat not valid";
        d->setError(Audio::OpenError);
        return false;
    }

    if (!d->isFormatSupported(d->format())) {
        qWarning() << "AudioSource::start: AudioFormat not supported by AudioDevice";
        d->setError(Audio::OpenError);
        return false;
    }
    return true;
};

void AudioSource::start(QIODevice* device)
{
    if (!d)
        return;

    d->setError(Audio::NoError);

    if (!device->isWritable()) {
        qWarning() << "AudioSource::start: QIODevice is not writable";
        d->setError(Audio::OpenError);
        return;
    }

    if (!validateFormatAtStart(d))
        return;

    d->elapsedTime.start();
    d->start(device);
}

QIODevice* AudioSource::start()
{
    if (!d)
        return nullptr;

    d->setError(Audio::NoError);

    if (!validateFormatAtStart(d))
        return nullptr;

    d->elapsedTime.start();
    return d->start();
}

AudioFormat AudioSource::format() const
{
    return d ? d->format() : AudioFormat();
}

void AudioSource::stop()
{
    if (d)
        d->stop();
}

void AudioSource::reset()
{
    if (d)
        d->reset();
}

void AudioSource::suspend()
{
    if (d)
        d->suspend();
}

void AudioSource::resume()
{
    if (d)
        d->resume();
}

void AudioSource::setBufferSize(qsizetype value)
{
    if (d)
        d->setBufferSize(value);
}

qsizetype AudioSource::bufferSize() const
{
    return d ? d->bufferSize() : 0;
}

void AudioSource::setBufferFrameCount(qsizetype value)
{
    if (d)
        setBufferSize(d->format().bytesForFrames(value));
}

qsizetype AudioSource::bufferFrameCount() const
{
    return d ? d->format().framesForBytes(bufferSize()) : 0;
}

qsizetype AudioSource::bytesAvailable() const
{
    return d ? d->bytesReady() : 0;
}

qsizetype AudioSource::framesAvailable() const
{

    return d ? d->format().framesForBytes(bytesAvailable()) : 0;
}

void AudioSource::setVolume(qreal volume)
{
    if (!d)
        return;

    std::optional<float> newVolume = AudioHelperInternal::sanitizeVolume(volume, this->volume());
    if (newVolume)
        d->setVolume(*newVolume);
}

qreal AudioSource::volume() const
{
    return d ? d->volume() : 1.0;
}

qint64 AudioSource::processedUSecs() const
{
    return d ? d->processedUSecs() : 0;
}

qint64 AudioSource::elapsedUSecs() const
{
    return state() == Audio::StoppedState ? 0 : d->elapsedTime.nsecsElapsed()/1000;
}

QtAudio::Error AudioSource::error() const
{
    return d ? d->error() : Audio::OpenError;
}

QtAudio::State AudioSource::state() const
{
    return d ? d->state() : Audio::StoppedState;
}

#include "moc_AudioSource.cpp"

