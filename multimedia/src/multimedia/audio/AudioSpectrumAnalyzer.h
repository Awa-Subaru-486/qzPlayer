// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#ifndef QT_AUDIO_AUDIOSPECTRUMANALYZER_H
#define QT_AUDIO_AUDIOSPECTRUMANALYZER_H

#include <QtCore/qobject.h>
#include <QtCore/qvector.h>
#include <qzMultimedia/MultimediaGlobal.h>

class AudioBuffer;

// 音频频谱分析器：接收 AudioBuffer，分声道执行 FFT 并输出频谱数据
class QZ_MULTIMEDIA_EXPORT AudioSpectrumAnalyzer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int fftSize READ fftSize WRITE setFftSize NOTIFY fftSizeChanged)
    Q_PROPERTY(int spectrumCount READ spectrumCount NOTIFY spectrumCountChanged)
    Q_PROPERTY(float smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
public:
    explicit AudioSpectrumAnalyzer(QObject *parent = nullptr);
    ~AudioSpectrumAnalyzer() override;

    int fftSize() const;
    void setFftSize(int size);

    int spectrumCount() const;

    float smoothing() const;
    void setSmoothing(float factor);

    // 处理 AudioBuffer，分声道执行 FFT
    void processBuffer(const AudioBuffer &buffer);

    // 获取左声道频谱数据（归一化 0~1）
    const QVector<float> &leftSpectrumData() const;
    // 获取右声道频谱数据（归一化 0~1），单声道时与左声道相同
    const QVector<float> &rightSpectrumData() const;

Q_SIGNALS:
    void spectrumUpdated();
    void fftSizeChanged();
    void spectrumCountChanged();
    void smoothingChanged();

private:
    void rebuildFftConfig();
    void performFft(const float *input, QVector<float> &output, QVector<float> &smoothed);

    int m_fftSize = 1024;
    float m_smoothing = 0.7f;

    void *m_fftCfg = nullptr;

    QVector<float> m_window;
    QVector<float> m_inputBuffer;

    QVector<float> m_leftSpectrum;
    QVector<float> m_rightSpectrum;
    QVector<float> m_leftSmoothed;
    QVector<float> m_rightSmoothed;
};

#endif
