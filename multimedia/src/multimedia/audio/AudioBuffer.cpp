// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioBuffer.h"

#include <QObject>
#include <QDebug>

class AudioBufferPrivate : public QSharedData
{
public:
    AudioBufferPrivate(const AudioFormat &f, const QByteArray &d, qint64 start)
        : format(f), data(d), startTime(start)
    {
    }

    AudioFormat format;
    QByteArray data;
    qint64 startTime;
};

QT_DEFINE_QESDP_SPECIALIZATION_DTOR(AudioBufferPrivate);

AudioBuffer::AudioBuffer() noexcept = default;

AudioBuffer::AudioBuffer(const AudioBuffer &other) noexcept = default;

AudioBuffer::AudioBuffer(const QByteArray &data, const AudioFormat &format, qint64 startTime)
{
    if (!format.isValid() || !data.size())
        return;
    d = new AudioBufferPrivate(format, data, startTime);
}

AudioBuffer::AudioBuffer(int numFrames, const AudioFormat &format, qint64 startTime)
{
    if (!format.isValid() || !numFrames)
        return;

    QByteArray data(format.bytesForFrames(numFrames), '\0');
    d = new AudioBufferPrivate(format, data, startTime);
}

AudioBuffer &AudioBuffer::operator=(const AudioBuffer &other) = default;

AudioBuffer::~AudioBuffer() = default;

void AudioBuffer::detach()
{
    if (!d)
        return;
    d = new AudioBufferPrivate(*d);
}

AudioFormat AudioBuffer::format() const noexcept
{
    if (!d)
        return AudioFormat();
    return d->format;
}

qsizetype AudioBuffer::frameCount() const noexcept
{
    if (!d)
        return 0;
    return d->format.framesForBytes(d->data.size());
}

qsizetype AudioBuffer::sampleCount() const noexcept
{
    return frameCount() * format().channelCount();
}

qsizetype AudioBuffer::byteCount() const noexcept
{
    return d ? d->data.size() : 0;
}

qint64 AudioBuffer::duration() const noexcept
{
    return format().durationForFrames(frameCount());
}

qint64 AudioBuffer::startTime() const noexcept
{
    if (!d)
        return -1;
    return d->startTime;
}

const void *AudioBuffer::constData() const noexcept
{
    if (!d)
        return nullptr;
    return d->data.constData();
}

const void *AudioBuffer::data() const noexcept
{
    if (!d)
        return nullptr;
    return d->data.constData();
}

void *AudioBuffer::data()
{
    if (!d)
        return nullptr;
    return d->data.data();
}

