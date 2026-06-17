// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

module;

#include <QtQml/qqmlengine.h>
#include <qqml.h>

#include "QuickVideoOutput_p.h"
#include "QuickAudioVisualizer_p.h"
#include "QuickPreviewFrame_p.h"
#include "qzmultimediaquickexports.h"

#include <qzMultimedia/MediaPlayer.h>
#include <qzMultimedia/AudioBufferOutput.h>
#include <qzMultimedia/AudioOutput.h>
#include <qzMultimedia/SubtitleStyle.h>
#include <qzMultimedia/TrackInfo.h>
#include <qzMultimedia/ChapterInfo.h>
#include <qzMultimedia/MediaMetadata.h>


export module qml_register_types_qzMultimedia;

import qzMultimediaQuick;

namespace qz {
    constexpr auto qz_multimedia_uri = "qz.multimedia";
    constexpr int version_major = 1;
    constexpr int version_minor = 0;
#define qz_multimedia_url_ma_mi qz_multimedia_uri, version_major, version_minor
}

export namespace qz
{
    QZ_MULTIMEDIAQUICK_EXPORT auto qml_register_types_qz_multimedia(QQmlEngine *) -> void
    {
        // QML 组件（QObject 派生类）
        qmlRegisterType<MediaPlayer>(qz_multimedia_url_ma_mi, "MediaPlayer");
        qmlRegisterType<QuickVideoOutput>(qz_multimedia_url_ma_mi, "VideoOutput");
        qmlRegisterType<QuickVideoSink>(qz_multimedia_url_ma_mi, "VideoSink");
        qmlRegisterType<QuickAudioVisualizer>(qz_multimedia_url_ma_mi, "AudioVisualizer");
        qmlRegisterType<QuickPreviewFrame>(qz_multimedia_url_ma_mi, "PreviewFrame");
        qmlRegisterType<AudioBufferOutput>(qz_multimedia_url_ma_mi, "AudioBufferOutput");
        qmlRegisterType<AudioOutput>(qz_multimedia_url_ma_mi, "AudioOutput");

        // Q_GADGET 值类型（不可在 QML 中创建，仅用于属性传递）
        qmlRegisterUncreatableType<TrackInfo>(qz_multimedia_url_ma_mi, "trackInfo", QStringLiteral("TrackInfo is a value type"));
        qmlRegisterUncreatableType<ChapterInfo>(qz_multimedia_url_ma_mi, "chapterInfo", QStringLiteral("ChapterInfo is a value type"));
        qmlRegisterUncreatableType<SubtitleStyle>(qz_multimedia_url_ma_mi, "subtitleStyle", QStringLiteral("SubtitleStyle is a value type"));
        qmlRegisterUncreatableType<MediaMetaData>(qz_multimedia_url_ma_mi, "mediaMetaData", QStringLiteral("MediaMetaData is a value type"));

        // QAbstractListModel 子类
        qmlRegisterType<TrackInfoModel>(qz_multimedia_url_ma_mi, "TrackInfoModel");

        // 注册 QList 序列类型，使 QML 可以遍历
        qRegisterMetaType<QList<TrackInfo>>();
        qRegisterMetaType<QList<ChapterInfo>>();
    }
}
