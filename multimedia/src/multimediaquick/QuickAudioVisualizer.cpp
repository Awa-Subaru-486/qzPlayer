// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "QuickAudioVisualizer_p.h"
#include "SGAudioVisualizerNode_p.h"
#include <qzMultimedia/AudioBufferOutput.h>
#include <qzMultimedia/AudioSpectrumAnalyzer.h>
#include <qzMultimedia/AudioBuffer.h>

QT_BEGIN_NAMESPACE

QuickAudioVisualizer::QuickAudioVisualizer(QQuickItem *parent)
    : QQuickItem(parent)
    , m_analyzer(new AudioSpectrumAnalyzer(this))
{
    setFlag(ItemHasContents);

    connect(m_analyzer, &AudioSpectrumAnalyzer::spectrumUpdated, this, [this]() {
        m_spectrumChanged = true;
        update();
    });
}

QuickAudioVisualizer::~QuickAudioVisualizer() = default;

AudioBufferOutput *QuickAudioVisualizer::audioBufferOutput() const
{
    return m_audioBufferOutput;
}

void QuickAudioVisualizer::setAudioBufferOutput(AudioBufferOutput *output)
{
    if (m_audioBufferOutput == output)
        return;

    if (m_audioBufferOutput)
        disconnect(m_audioBufferOutput, nullptr, this, nullptr);

    m_audioBufferOutput = output;

    if (m_audioBufferOutput) {
        connect(m_audioBufferOutput, &AudioBufferOutput::audioBufferReceived,
                this, &QuickAudioVisualizer::handleAudioBuffer);
    }

    emit audioBufferOutputChanged();
}

QuickAudioVisualizer::VisualizationStyle QuickAudioVisualizer::style() const
{
    return m_style;
}

void QuickAudioVisualizer::setStyle(VisualizationStyle style)
{
    if (m_style == style)
        return;
    m_style = style;
    m_spectrumChanged = true;
    update();
    emit styleChanged();
}

int QuickAudioVisualizer::barCount() const
{
    return m_barCount;
}

void QuickAudioVisualizer::setBarCount(int count)
{
    if (m_barCount == count)
        return;
    m_barCount = count;
    m_spectrumChanged = true;
    update();
    emit barCountChanged();
}

QColor QuickAudioVisualizer::color() const
{
    return m_color;
}

void QuickAudioVisualizer::setColor(const QColor &color)
{
    if (m_color == color)
        return;
    m_color = color;
    m_spectrumChanged = true;
    update();
    emit colorChanged();
}

int QuickAudioVisualizer::fftSize() const
{
    return m_analyzer->fftSize();
}

void QuickAudioVisualizer::setFftSize(int size)
{
    if (m_analyzer->fftSize() == size)
        return;
    m_analyzer->setFftSize(size);
    emit fftSizeChanged();
}

float QuickAudioVisualizer::smoothing() const
{
    return m_analyzer->smoothing();
}

void QuickAudioVisualizer::setSmoothing(float factor)
{
    if (qFuzzyCompare(m_analyzer->smoothing(), factor))
        return;
    m_analyzer->setSmoothing(factor);
    emit smoothingChanged();
}

QSGNode *QuickAudioVisualizer::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    auto *node = static_cast<SGAudioVisualizerNode *>(oldNode);
    if (!node)
        node = new SGAudioVisualizerNode();

    if (m_geometryChanged) {
        node->setRect(boundingRect());
        m_geometryChanged = false;
    }

    if (m_spectrumChanged) {
        const auto &leftData = m_analyzer->leftSpectrumData();
        const auto &rightData = m_analyzer->rightSpectrumData();
        if (!leftData.isEmpty()) {
            node->setSpectrumData(leftData.constData(), leftData.size(),
                                  rightData.constData(), rightData.size());
            node->setColor(m_color);
        }
        m_spectrumChanged = false;
    }

    return node;
}

void QuickAudioVisualizer::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    m_geometryChanged = true;
    update();
}

void QuickAudioVisualizer::releaseResources()
{
    m_audioBufferOutput = nullptr;
}

void QuickAudioVisualizer::handleAudioBuffer(const AudioBuffer &buffer)
{
    m_analyzer->processBuffer(buffer);
}

QT_END_NAMESPACE
