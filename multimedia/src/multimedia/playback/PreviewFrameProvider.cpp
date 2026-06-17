// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "PreviewFrameProvider.h"
#include "PreviewFrameProvider_p.h"

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QPromise>
#include <QtCore/QFuture>

Q_DECLARE_METATYPE(PreviewFrameData)

QT_BEGIN_NAMESPACE

PreviewFrameProvider::PreviewFrameProvider(QObject *parent)
    : QObject(parent)
{
    m_backend = PreviewFrameProviderPrivate::createBackend();
}

PreviewFrameProvider::~PreviewFrameProvider()
{
    delete m_backend;
}

void PreviewFrameProvider::setSource(const QUrl &source)
{
    if (m_backend)
        m_backend->setSource(source);
}

QFuture<PreviewFrameData> PreviewFrameProvider::requestFrame(qint64 positionMs, const QSize &maxSize)
{
    auto promise = std::make_shared<QPromise<PreviewFrameData>>();
    promise->start();
    auto future = promise->future();

    if (!m_backend)
    {
        promise->addResult(PreviewFrameData{});
        promise->finish();
        return future;
    }

    m_backend->requestFrame(positionMs, maxSize, [promise](const PreviewFrameData &data) {
        promise->addResult(data);
        promise->finish();
    });

    return future;
}

void PreviewFrameProvider::cancel()
{
    if (m_backend)
        m_backend->cancel();
}

PlatformPreviewFrameProvider *PreviewFrameProviderPrivate::createBackend()
{
    auto *integration = PlatformMediaIntegration::instance();
    if (!integration)
        return nullptr;
    return integration->createPreviewFrameProvider();
}

QT_END_NAMESPACE

#include "moc_PreviewFrameProvider.cpp"
