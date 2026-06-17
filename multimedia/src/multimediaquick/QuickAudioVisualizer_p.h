// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QUICKAUDIOVISUALIZER_P_H
#define QUICKAUDIOVISUALIZER_P_H

#include <QtQuick/qquickitem.h>
#include <QtCore/qpointer.h>
#include <qzMultimedia/AudioBufferOutput.h>
#include <qzmultimediaquickexports.h>

QT_BEGIN_NAMESPACE

class AudioSpectrumAnalyzer;
class SGAudioVisualizerNode;

// QML 音频可视化组件：实时渲染音频频谱
class QZ_MULTIMEDIAQUICK_EXPORT QuickAudioVisualizer : public QQuickItem
{
    Q_OBJECT
    Q_DISABLE_COPY(QuickAudioVisualizer)
    Q_PROPERTY(AudioBufferOutput *audioBufferOutput READ audioBufferOutput
               WRITE setAudioBufferOutput NOTIFY audioBufferOutputChanged)
    Q_PROPERTY(VisualizationStyle style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(int barCount READ barCount WRITE setBarCount NOTIFY barCountChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(int fftSize READ fftSize WRITE setFftSize NOTIFY fftSizeChanged)
    Q_PROPERTY(float smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)

public:
    enum VisualizationStyle
    {
        BarSpectrum,     // 柱状频谱
        Waveform,        // 波形
        CircularSpectrum // 圆形频谱
    };
    Q_ENUM(VisualizationStyle)

    explicit QuickAudioVisualizer(QQuickItem *parent = nullptr);
    ~QuickAudioVisualizer() override;

    AudioBufferOutput *audioBufferOutput() const;
    void setAudioBufferOutput(AudioBufferOutput *output);

    VisualizationStyle style() const;
    void setStyle(VisualizationStyle style);

    int barCount() const;
    void setBarCount(int count);

    QColor color() const;
    void setColor(const QColor &color);

    int fftSize() const;
    void setFftSize(int size);

    float smoothing() const;
    void setSmoothing(float factor);

Q_SIGNALS:
    void audioBufferOutputChanged();
    void styleChanged();
    void barCountChanged();
    void colorChanged();
    void fftSizeChanged();
    void smoothingChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;
    void releaseResources() override;

private:
    void handleAudioBuffer(const AudioBuffer &buffer);

    QPointer<AudioBufferOutput> m_audioBufferOutput;
    AudioSpectrumAnalyzer *m_analyzer = nullptr;

    VisualizationStyle m_style = BarSpectrum;
    int m_barCount = 64;
    QColor m_color = QColor(0, 150, 255);
    bool m_spectrumChanged = false;
    bool m_geometryChanged = true;
};

QT_END_NAMESPACE

#endif
