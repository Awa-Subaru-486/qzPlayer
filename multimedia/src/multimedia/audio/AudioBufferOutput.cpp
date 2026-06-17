// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioBufferOutput_p.h"
#include "MediaPlayer.h"
#include "AudioBuffer.h"

AudioBufferOutput::AudioBufferOutput(QObject *parent)
    : QObject(*new AudioBufferOutputPrivate, parent)
{
}

AudioBufferOutput::AudioBufferOutput(const AudioFormat &format, QObject *parent)
    : QObject(*new AudioBufferOutputPrivate(format), parent)
{
}

AudioBufferOutput::~AudioBufferOutput()
{
    Q_D(AudioBufferOutput);

    if (d->mediaPlayer)
        d->mediaPlayer->setAudioBufferOutput(nullptr);
}

AudioFormat AudioBufferOutput::format() const
{
    Q_D(const AudioBufferOutput);
    return d->format;
}

#include "moc_AudioBufferOutput.cpp"
