// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_ANDROID_AAUDIOSTREAM_P_H
#define QT_ANDROID_AAUDIOSTREAM_P_H

#include <qzMultimedia/AudioFormat.h>
import qzLog;

#include <aaudio/AAudio.h>

QT_BEGIN_NAMESPACE

namespace QtAAudio {

extern qz::Log::LogCategory qLcAAudioStream;

struct StreamBuilder;

struct Stream
{
    explicit Stream(StreamBuilder &builder);
    ~Stream();

    Q_DISABLE_COPY_MOVE(Stream)

    bool start();
    void stop();
    void pause();
    void flush();

    bool isOpen() const;
    bool areStreamParametersRespected() const;

private:
    void close();

    aaudio_result_t waitForTargetState(aaudio_stream_state_t targetState);

    template <typename Functor>
    aaudio_result_t requestWithExpectedState(Functor &&request, aaudio_stream_state_t expected);

    AAudioStream *m_stream{ nullptr };
    bool m_areStreamParametersRespected{ false };
};

struct StreamParameterSet
{
    aaudio_sharing_mode_t sharingMode = AAUDIO_SHARING_MODE_SHARED;
    aaudio_direction_t direction = AAUDIO_DIRECTION_OUTPUT;
    aaudio_usage_t outputUsage = AAUDIO_USAGE_MEDIA;
    aaudio_content_type_t outputContentType = AAUDIO_CONTENT_TYPE_MUSIC;
    aaudio_input_preset_t inputPreset = AAUDIO_INPUT_PRESET_VOICE_RECOGNITION;
};

struct StreamBuilder
{
    friend Stream;

    StreamBuilder(AudioFormat format);
    ~StreamBuilder();

    Q_DISABLE_COPY_MOVE(StreamBuilder)

    AudioFormat format;
    AAudioStream_dataCallback callback;
    AAudioStream_errorCallback errorCallback;
    void *userData{ nullptr };
    StreamParameterSet params;
    int32_t deviceId{ AAUDIO_UNSPECIFIED };
    int32_t bufferCapacity{ AAUDIO_UNSPECIFIED };

    void setupBuilder();

private:
    AAudioStreamBuilder *m_builder{ nullptr };
};

} // namespace QtAAudio

QT_END_NAMESPACE

#endif
