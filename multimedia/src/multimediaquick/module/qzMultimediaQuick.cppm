// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

module;

#include <QtQml/qqml.h>
#include <QtQuick/qquickitem.h>

#include "QuickVideoOutput_p.h"
#include "QuickAudioVisualizer_p.h"
#include "QuickPreviewFrame_p.h"

export module qzMultimediaQuick;

export namespace qz
{
    using QuickVideoOutput = QT_PREPEND_NAMESPACE(QuickVideoOutput);
    using QuickVideoSink = QT_PREPEND_NAMESPACE(QuickVideoSink);
    using QuickAudioVisualizer = QT_PREPEND_NAMESPACE(QuickAudioVisualizer);
    using QuickPreviewFrame = QT_PREPEND_NAMESPACE(QuickPreviewFrame);
}
