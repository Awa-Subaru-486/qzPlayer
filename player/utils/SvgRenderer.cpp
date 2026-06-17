#include "SvgRenderer.hpp"
#include <QFile>
#include <QSGSimpleTextureNode>
#include <QQuickWindow>
#include <QUrl>
#include <lunasvg.h>
#include <QtConcurrent>

import qzLog;

namespace qz {

SvgRenderer::SvgRenderer(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setFlag(ItemAcceptsInputMethod, false);

    connect(&m_watcher, &QFutureWatcher<SvgAsyncResult>::finished,
            this, &SvgRenderer::onAsyncRenderFinished);
}

SvgRenderer::~SvgRenderer()
{
    disconnect(&m_watcher, &QFutureWatcher<SvgAsyncResult>::finished,
               this, &SvgRenderer::onAsyncRenderFinished);
    m_watcher.cancel();
    m_watcher.waitForFinished();
}

QString SvgRenderer::source() const
{
    return m_source;
}

void SvgRenderer::set_source(const QString& in_source)
{
    if (m_source == in_source)
        return;

    m_source = in_source;
    m_masterCache = QImage();
    m_renderCache = QImage();
    m_docWidth = 0;
    m_docHeight = 0;

    updateRenderCache();
    updateImplicitSizeFromDocument();

    update();
    emit sourceChanged();
}

QColor SvgRenderer::color() const
{
    return m_color;
}

auto SvgRenderer::set_paintedSize(int in_paintedSize) -> void
{
    if (m_paintedSize == in_paintedSize)
        return;

    m_paintedSize = in_paintedSize;
    updateRenderCache();
    update();
    emit paintedSizeChanged();
}

auto SvgRenderer::paintedSize() const -> int
{
    return m_paintedSize;
}

auto SvgRenderer::set_useOriginalColor(bool in_useOriginalColor) -> void
{
    if (m_useOriginalColor == in_useOriginalColor)
        return;

    m_useOriginalColor = in_useOriginalColor;
    updateRenderCache();
    update();

    emit useOriginalColorChanged();
}

auto SvgRenderer::useOriginalColor() const -> bool
{
    return m_useOriginalColor;
}

void SvgRenderer::set_color(const QColor& in_color)
{
    if (m_color == in_color)
        return;

    m_color = in_color;
    updateRenderCache();

    update();
    emit colorChanged();
}

void SvgRenderer::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);

    if (!qFuzzyCompare(newGeometry.width(), oldGeometry.width()) ||
        !qFuzzyCompare(newGeometry.height(), oldGeometry.height())) {
        updateRenderCache();
    }
}

void SvgRenderer::updateImplicitSizeFromDocument()
{
    if (m_docWidth > 0 && m_docHeight > 0) {
        setImplicitWidth(m_docWidth);
        setImplicitHeight(m_docHeight);
    } else {
        setImplicitWidth(0);
        setImplicitHeight(0);
    }
}

void SvgRenderer::updateRenderCache()
{
    if (m_source.isEmpty()) {
        m_masterCache = QImage();
        m_renderCache = QImage();
        return;
    }

    ++m_generation;
    const int gen = m_generation.load();

    const QString source = m_source;
    const QColor color = m_color;
    const bool useOriginalColor = m_useOriginalColor;
    const qreal dpr = window() ? window()->devicePixelRatio() : 1.0;

    int targetW = 0;
    int targetH = 0;

    if (m_paintedSize > 0) {
        targetW = m_paintedSize;
        targetH = m_paintedSize;
    } else {
        targetW = width() > 0 ? static_cast<int>(width()) : 0;
        targetH = height() > 0 ? static_cast<int>(height()) : 0;
    }

    constexpr int defSize = 32;
    if (targetW <= 0) targetW = defSize;
    if (targetH <= 0) targetH = defSize;

    const int renderW = static_cast<int>(targetW * dpr);
    const int renderH = static_cast<int>(targetH * dpr);
    const QSize currentSize(renderW, renderH);

    const QImage masterCopy = m_masterCache;
    const bool hasValidMaster = !masterCopy.isNull() && masterCopy.size() == currentSize;

    auto future = QtConcurrent::run([source, color, useOriginalColor, dpr,
                                      renderW, renderH, currentSize,
                                      masterCopy, hasValidMaster, gen]() -> SvgAsyncResult {
        SvgAsyncResult result;
        result.generation = gen;

        QImage masterCache;

        if (hasValidMaster) {
            masterCache = masterCopy;
        } else {
            QString filePath;
            const QUrl url(source);
            if (url.scheme() == "qrc") {
                filePath = ":" + url.path();
            } else if (url.isLocalFile()) {
                filePath = url.toLocalFile();
            } else if (source.startsWith("/res/")) {
                filePath = ":/qt/qml/qz/player" + source;
            } else {
                filePath = source;
                if (!filePath.startsWith(":/"))
                    filePath.prepend(":/");
            }

            // Qt 6 QML resource path convention: :/xxx → :/qt/qml/xxx
            if (!QFile::exists(filePath) && filePath.startsWith(":/") && !filePath.startsWith(":/qt/qml/")) {
                const QString altPath = ":/qt/qml/" + filePath.mid(2);
                if (QFile::exists(altPath))
                    filePath = altPath;
            }

            bool loaded = false;
            std::unique_ptr<lunasvg::Document> doc;

            if (QFile file(filePath); file.open(QIODevice::ReadOnly)) {
                const QByteArray rawData = file.readAll();
                const std::string svgData = rawData.toStdString();
                file.close();
                if (!svgData.empty()) {
                    doc = lunasvg::Document::loadFromData(svgData);
                    loaded = true;
                }
            }

            if (!loaded && !source.isEmpty()) {
                doc = lunasvg::Document::loadFromData(source.toStdString());
            }

            if (!doc) {
                result.loadFailed = true;
                return result;
            }

            result.docWidth = doc->width();
            result.docHeight = doc->height();

            const auto bitmap = doc->renderToBitmap(renderW, renderH, 0x00000000u);
            if (bitmap.isNull()) {
                result.renderFailed = true;
                return result;
            }

            const QImage tmp(
                bitmap.data(),
                bitmap.width(),
                bitmap.height(),
                bitmap.stride(),
                QImage::Format_ARGB32_Premultiplied
            );
            masterCache = tmp.copy();
            masterCache.setDevicePixelRatio(dpr);
        }

        result.masterCache = masterCache;
        result.success = true;

        if (useOriginalColor) {
            result.renderCache = masterCache;
        } else {
            result.renderCache = masterCache.copy();
            result.renderCache.setDevicePixelRatio(dpr);
            applyColorToImage(result.renderCache, color);
        }

        return result;
    });

    m_watcher.setFuture(future);
}

void SvgRenderer::onAsyncRenderFinished()
{
    const SvgAsyncResult result = m_watcher.result();

    if (result.generation != m_generation.load()) {
        return;
    }

    if (result.success) {
        m_masterCache = result.masterCache;
        m_renderCache = result.renderCache;

        if (result.docWidth > 0 || result.docHeight > 0) {
            m_docWidth = result.docWidth;
            m_docHeight = result.docHeight;
            updateImplicitSizeFromDocument();
        }
    } else {
        if (result.loadFailed) {
            Log::warn("failed to load SVG: {}", m_source);
        }
        if (result.renderFailed) {
            Log::warn("renderToBitmap failed");
        }
        m_masterCache = QImage();
        m_renderCache = QImage();
    }

    update();
}

void SvgRenderer::applyColorToImage(QImage &image, const QColor &color)
{
    if (image.isNull() || !color.isValid())
        return;

    if (image.format() == QImage::Format_ARGB32_Premultiplied) {
        image = image.convertToFormat(QImage::Format_ARGB32);
    }

    const int w = image.width();
    const int h = image.height();

    const int r = color.red();
    const int g = color.green();
    const int b = color.blue();
    const int a = color.alpha();

    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            QRgb pixel = line[x];
            int srcAlpha = qAlpha(pixel);

            if (srcAlpha == 0) {
                continue;
            }

            const int finalAlpha = (srcAlpha * a) / 255;
            line[x] = qRgba(r, g, b, finalAlpha);
        }
    }

    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}

QSGNode *SvgRenderer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setOwnsTexture(true);
    }

    if (!m_renderCache.isNull() && window()) {
        QSGTexture *texture = window()->createTextureFromImage(m_renderCache);
        if (!texture) {
            Log::warn("createTextureFromImage failed");
            delete node;
            return nullptr;
        }

        node->setTexture(texture);
        node->setRect(boundingRect());
        node->setFiltering(QSGTexture::Linear);
        return node;
    }

    delete node;
    return nullptr;
}

}
