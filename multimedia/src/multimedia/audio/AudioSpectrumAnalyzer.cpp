// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "AudioSpectrumAnalyzer.h"
#include "AudioBuffer.h"
#include "AudioFormat.h"

#include <kiss_fft.h>

#include <cmath>

static QVector<float> createHanningWindow(int size)
{
    QVector<float> window(size);
    for (int i = 0; i < size; ++i)
        window[i] = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (size - 1)));
    return window;
}

AudioSpectrumAnalyzer::AudioSpectrumAnalyzer(QObject *parent)
    : QObject(parent)
{
    rebuildFftConfig();
}

AudioSpectrumAnalyzer::~AudioSpectrumAnalyzer()
{
    if (m_fftCfg)
        kiss_fft_free(m_fftCfg);
}

int AudioSpectrumAnalyzer::fftSize() const
{
    return m_fftSize;
}

void AudioSpectrumAnalyzer::setFftSize(int size)
{
    if (m_fftSize == size)
        return;
    m_fftSize = size;
    rebuildFftConfig();
    emit fftSizeChanged();
    emit spectrumCountChanged();
}

int AudioSpectrumAnalyzer::spectrumCount() const
{
    return m_fftSize / 2;
}

float AudioSpectrumAnalyzer::smoothing() const
{
    return m_smoothing;
}

void AudioSpectrumAnalyzer::setSmoothing(float factor)
{
    if (qFuzzyCompare(m_smoothing, factor))
        return;
    m_smoothing = factor;
    emit smoothingChanged();
}

void AudioSpectrumAnalyzer::rebuildFftConfig()
{
    if (m_fftCfg) {
        kiss_fft_free(m_fftCfg);
        m_fftCfg = nullptr;
    }

    m_fftCfg = kiss_fft_alloc(m_fftSize, 0, nullptr, nullptr);
    m_window = createHanningWindow(m_fftSize);

    int halfSize = m_fftSize / 2;
    m_inputBuffer.resize(m_fftSize);
    m_leftSpectrum.resize(halfSize);
    m_rightSpectrum.resize(halfSize);
    m_leftSmoothed.resize(halfSize);
    m_rightSmoothed.resize(halfSize);

    m_leftSpectrum.fill(0);
    m_rightSpectrum.fill(0);
    m_leftSmoothed.fill(0);
    m_rightSmoothed.fill(0);
}

void AudioSpectrumAnalyzer::performFft(const float *input, QVector<float> &output, QVector<float> &smoothed)
{
    // 应用窗函数
    m_inputBuffer.resize(m_fftSize);
    for (int i = 0; i < m_fftSize; ++i)
        m_inputBuffer[i] = input[i] * m_window[i];

    // 准备 FFT 输入
    QVector<kiss_fft_cpx> fftIn(m_fftSize);
    QVector<kiss_fft_cpx> fftOut(m_fftSize);
    for (int i = 0; i < m_fftSize; ++i) {
        fftIn[i].r = m_inputBuffer[i];
        fftIn[i].i = 0.f;
    }

    // 执行 FFT
    kiss_fft(static_cast<kiss_fft_cfg>(m_fftCfg), fftIn.data(), fftOut.data());

    // 计算幅度谱（取前半部分，归一化到 0~1）
    const int halfSize = m_fftSize / 2;
    const float norm = 2.f / m_fftSize;
    for (int i = 0; i < halfSize; ++i) {
        float re = fftOut[i].r;
        float im = fftOut[i].i;
        float magnitude = std::sqrt(re * re + im * im) * norm;
        // 转换为 dB 并归一化（-60dB ~ 0dB 映射到 0~1）
        float db = 20.f * std::log10(magnitude + 1e-10f);
        output[i] = qBound(0.f, (db + 60.f) / 60.f, 1.f);
    }

    // 应用 EMA 平滑
    for (int i = 0; i < halfSize; ++i)
        smoothed[i] = m_smoothing * smoothed[i] + (1.f - m_smoothing) * output[i];
}

void AudioSpectrumAnalyzer::processBuffer(const AudioBuffer &buffer)
{
    if (!buffer.isValid() || !m_fftCfg)
        return;

    const AudioFormat format = buffer.format();
    const int channelCount = format.channelCount();
    const int frameCount = int(buffer.frameCount());
    const int samplesToFill = qMin(frameCount, m_fftSize);

    // 提取左声道和右声道数据
    QVector<float> leftChannel(m_fftSize, 0.f);
    QVector<float> rightChannel(m_fftSize, 0.f);

    if (format.sampleFormat() == AudioFormat::Float) {
        const float *src = buffer.constData<float>();
        for (int i = 0; i < samplesToFill; ++i) {
            leftChannel[i] = src[i * channelCount];
            rightChannel[i] = (channelCount > 1) ? src[i * channelCount + 1] : src[i * channelCount];
        }
    } else if (format.sampleFormat() == AudioFormat::Int16) {
        const short *src = buffer.constData<short>();
        for (int i = 0; i < samplesToFill; ++i) {
            leftChannel[i] = float(src[i * channelCount]) / 32768.f;
            rightChannel[i] = (channelCount > 1) ? float(src[i * channelCount + 1]) / 32768.f : leftChannel[i];
        }
    } else if (format.sampleFormat() == AudioFormat::Int32) {
        const int *src = buffer.constData<int>();
        for (int i = 0; i < samplesToFill; ++i) {
            leftChannel[i] = float(src[i * channelCount]) / 2147483648.f;
            rightChannel[i] = (channelCount > 1) ? float(src[i * channelCount + 1]) / 2147483648.f : leftChannel[i];
        }
    } else if (format.sampleFormat() == AudioFormat::UInt8) {
        const unsigned char *src = buffer.constData<unsigned char>();
        for (int i = 0; i < samplesToFill; ++i) {
            leftChannel[i] = (float(src[i * channelCount]) - 128.f) / 128.f;
            rightChannel[i] = (channelCount > 1) ? (float(src[i * channelCount + 1]) - 128.f) / 128.f : leftChannel[i];
        }
    } else {
        return;
    }

    // 不足部分填零（已在构造时填零）
    performFft(leftChannel.constData(), m_leftSpectrum, m_leftSmoothed);
    performFft(rightChannel.constData(), m_rightSpectrum, m_rightSmoothed);

    emit spectrumUpdated();
}

const QVector<float> &AudioSpectrumAnalyzer::leftSpectrumData() const
{
    return m_leftSmoothed;
}

const QVector<float> &AudioSpectrumAnalyzer::rightSpectrumData() const
{
    return m_rightSmoothed;
}
