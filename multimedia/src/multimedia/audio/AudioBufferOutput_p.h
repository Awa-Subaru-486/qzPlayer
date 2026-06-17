// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOBUFFEROUTPUT_P_H
#define QT_AUDIO_AUDIOBUFFEROUTPUT_P_H

#include <QtCore/private/qobject_p.h>
#include "AudioBuffer.h"
#include "AudioBufferOutput.h"

class MediaPlayer;

class AudioBufferOutputPrivate : public QObjectPrivate
{
public:
    explicit AudioBufferOutputPrivate(const AudioFormat &format = {}) : format(format) { }

    static MediaPlayer *exchangeMediaPlayer(AudioBufferOutput &output, MediaPlayer *player)
    {
        auto outputPrivate = static_cast<AudioBufferOutputPrivate *>(output.d_func());
        return std::exchange(outputPrivate->mediaPlayer, player);
    }

    AudioFormat format;
    MediaPlayer *mediaPlayer = nullptr;
};

#endif
