// Copyright (C) The Qt Company Ltd.
// Copyright (C) 2026 AwaSubaru
// SPDX-License-Identifier: LGPL-3.0-only
//
// This is a secondary development based on Qt Multimedia.

#include "WindowsResampler_p.h"

import qzLog;
#include <QtCore/private/qsystemerror_p.h>
#include <qzMultimedia/private/AudioAlignmentSupport_p.h>
#include <qzMultimedia/private/WindowsAudioUtils_p.h>
#include <qzMultimedia/private/WmfSupport_p.h>

#include <wmcodecdsp.h>
#include <mftransform.h>
#include <mferror.h>

static qz::Log::LogCategory qLcAudioResampler("qz.multimedia.audioresampler");

namespace {

HRESULT replaceBuffer(const ComPtr<IMFSample> &sample, const ComPtr<IMFMediaBuffer> &buffer)
{
    HRESULT hr = sample->RemoveAllBuffers();
    if (FAILED(hr))
        return hr;

    return sample->AddBuffer(buffer.Get());
}

}

bool WindowsResampler::isAvailable()
{
    return WindowsMediaFoundation::instance();
}

WindowsResampler::WindowsResampler()
{
    CoCreateInstance(__uuidof(CResamplerMediaObject), nullptr, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&m_resampler));
    if (m_resampler)
        m_resampler->AddInputStreams(1, &m_inputStreamID);

    for (ComPtr<IMFSample> &sample : { std::ref(m_inputSample), std::ref(m_outputSample) }) {
        HRESULT hr = m_wmf->mfCreateSample(sample.GetAddressOf());
        if (FAILED(hr)) {
            qz::Log::cat_warn(qLcAudioResampler, "Failed to create sample for resampling: {}", QSystemError::windowsComString(hr));
            m_resampler = nullptr;
            return;
        }
    }
}

WindowsResampler::~WindowsResampler() = default;

quint64 WindowsResampler::outputBufferSize(quint64 inputBufferSize) const
{
    if (m_inputFormat.isValid() && m_outputFormat.isValid())
        return m_outputFormat.bytesForDuration(m_inputFormat.durationForBytes(inputBufferSize));
    else
        return 0;
}

quint64 WindowsResampler::inputBufferSize(quint64 outputBufferSize) const
{
    if (m_inputFormat.isValid() && m_outputFormat.isValid())
        return m_inputFormat.bytesForDuration(m_outputFormat.durationForBytes(outputBufferSize));
    else
        return 0;
}

qsizetype WindowsResampler::overAllocatedOutputBufferSize()
{
    auto expectedOutputSize = outputBufferSize(m_totalInputBytes) - m_totalOutputBytes;

    expectedOutputSize += m_outputFormat.bytesForDuration(10000);
    expectedOutputSize = QtMultimediaPrivate::alignUp(expectedOutputSize, 1024);
    return expectedOutputSize;
}

template <typename Functor>
auto WindowsResampler::processOutput(ComPtr<IMFMediaBuffer> buffer, Functor &&f)
        -> std::invoke_result_t<Functor, const ComPtr<IMFMediaBuffer> &>
{
    HRESULT hr = replaceBuffer(m_outputSample, buffer);
    if (FAILED(hr))
        return std::unexpected{ hr };

    MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
    outputDataBuffer.dwStreamID = 0;
    outputDataBuffer.pEvents = nullptr;
    outputDataBuffer.dwStatus = 0;
    outputDataBuffer.pSample = m_outputSample.Get();
    DWORD status = 0;
    hr = m_resampler->ProcessOutput(0, 1, &outputDataBuffer, &status);
    if (FAILED(hr))
        return std::unexpected{ hr };

    return f(buffer);
}

std::expected<QByteArray, HRESULT> WindowsResampler::processOutput()
{
    using namespace WMF;

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = ByteArrayMFMediaBuffer::CreateInstance(overAllocatedOutputBufferSize(),
                                                         buffer.GetAddressOf());
    if (FAILED(hr))
        return std::unexpected{ hr };

    return processOutput(std::move(buffer), [&](const ComPtr<IMFMediaBuffer> &buffer) {
        return withLockedBuffer(buffer, [&](std::span<BYTE> data, std::span<BYTE> ) {
            auto *byteArrayBuffer = static_cast<ByteArrayMFMediaBuffer *>(buffer.Get());
            QByteArray out = byteArrayBuffer->takeByteArray();
            out.truncate(data.size());
            return out;
        });
    });
}

QByteArray WindowsResampler::resample(QByteArray in)
{
    m_totalInputBytes += in.size();

    if (m_inputFormat == m_outputFormat) {
        m_totalOutputBytes += in.size();
        return in;
    }

    Q_ASSERT(m_resampler && m_wmf);

    ComPtr<IMFMediaBuffer> buffer;

    WMF::ByteArrayMFMediaBuffer::CreateInstance(std::move(in), buffer.GetAddressOf(),
                                                  true);

    HRESULT hr = replaceBuffer(m_inputSample, buffer);
    if (FAILED(hr))
        return {};

    hr = m_resampler->ProcessInput(m_inputStreamID, m_inputSample.Get(), 0);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcAudioResampler, "Failed to process input {}", QSystemError::windowsComString(hr));
        return {};
    }

    auto result = processOutput();
    if (result) {
        m_totalOutputBytes += result.value().size();
        return result.value();
    }

    qz::Log::cat_warn(qLcAudioResampler, "Resampling failed {}", QSystemError::windowsComString(result.error()));
    return {};
}

QByteArray WindowsResampler::resample(const QByteArrayView &in)
{
    return WindowsResampler::resample(QByteArray(in));
}

QByteArray WindowsResampler::resample(const ComPtr<IMFSample> &sample)
{
    using namespace WMF;

    Q_ASSERT(sample);

    DWORD totalLength = 0;
    HRESULT hr = sample->GetTotalLength(&totalLength);
    if (FAILED(hr))
        return {};

    m_totalInputBytes += totalLength;

    if (m_inputFormat == m_outputFormat) {
        ComPtr<IMFMediaBuffer> outputBuffer;
        sample->ConvertToContiguousBuffer(outputBuffer.GetAddressOf());

        auto result = withLockedBuffer(outputBuffer, [&](std::span<BYTE> data, std::span<BYTE> ) {
            return QByteArray(data);
        });
        if (result) {
            m_totalOutputBytes += result.value().size();
            return result.value();
        }
        qz::Log::cat_warn(qLcAudioResampler, "Failed to convert sample to contiguous buffer {}", QSystemError::windowsComString(result.error()));
        return {};
    }

    Q_ASSERT(m_resampler && m_wmf);

    hr = m_resampler->ProcessInput(m_inputStreamID, sample.Get(), 0);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcAudioResampler, "Failed to process input sample {}", QSystemError::windowsComString(hr));
        return {};
    }

    auto result = processOutput();
    if (result) {
        m_totalOutputBytes += result.value().size();
        return result.value();
    }
    qz::Log::cat_warn(qLcAudioResampler, "Resampling failed {}", QSystemError::windowsComString(hr));
    return {};
}

AudioBuffer WindowsResampler::resample(const char *data, size_t size)
{
    quint64 elapsedBytesAtStart = m_totalOutputBytes;

    QByteArray resampled = resample(std::span{
            reinterpret_cast<const std::byte *>(data),
            static_cast<size_t>(size),
    });

    if (resampled.isEmpty())
        return {};

    return AudioBuffer{
        std::move(resampled),
        m_outputFormat,
        m_outputFormat.durationForBytes(elapsedBytesAtStart) + m_startTimeOffset.count(),
    };
}

std::pmr::vector<std::byte> WindowsResampler::resample(std::span<const std::byte> in,
                                                        std::pmr::memory_resource *mr)
{
    using namespace WMF;

    m_totalInputBytes += in.size_bytes();
    if (m_inputFormat == m_outputFormat) {
        m_totalOutputBytes += in.size();
        return std::pmr::vector<std::byte>{ in.begin(), in.end(), mr };
    }

    ComPtr<IMFMediaBuffer> inputBuffer;
    HRESULT hr = PmrMediaBuffer::CreateInstance(in, mr, inputBuffer.GetAddressOf());
    if (FAILED(hr))
        return {};

    hr = replaceBuffer(m_inputSample, inputBuffer);
    if (FAILED(hr))
        return {};

    hr = m_resampler->ProcessInput(m_inputStreamID, m_inputSample.Get(), 0);
    if (FAILED(hr))
        return {};

    ComPtr<IMFMediaBuffer> outputBuffer;
    hr = PmrMediaBuffer::CreateInstance(overAllocatedOutputBufferSize(), mr,
                                         outputBuffer.GetAddressOf());
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcAudioResampler, "Failed to create output buffer {}", QSystemError::windowsComString(hr));
        return {};
    }

    auto result = processOutput(std::move(outputBuffer), [&](const ComPtr<IMFMediaBuffer> &buffer) {
        return withLockedBuffer(buffer, [&](std::span<BYTE> data, std::span<BYTE> ) {
            return std::pmr::vector<std::byte>{
                reinterpret_cast<std::byte *>(data.data()),
                reinterpret_cast<std::byte *>(data.data() + data.size()),
                mr,
            };
        });
    });

    if (!result) {
        qz::Log::cat_warn(qLcAudioResampler, "Resampling failed {}", QSystemError::windowsComString(result.error()));
        return {};
    }
    m_totalOutputBytes += result->size();
    return result.value();
}

bool WindowsResampler::setup(const AudioFormat &fin, const AudioFormat &fout)
{
    qz::Log::cat_debug(qLcAudioResampler, "Setup audio resampler {} Hz {} ch -> {} Hz {} ch", fin.sampleRate(), fin.channelCount(), fout.sampleRate(), fout.channelCount());

    m_totalInputBytes = 0;
    m_totalOutputBytes = 0;

    if (fin == fout) {
        qz::Log::cat_debug(qLcAudioResampler, "Pass through mode");
        m_inputFormat = fin;
        m_outputFormat = fout;
        return true;
    }

    if (!m_resampler || !m_wmf)
        return false;

    ComPtr<IMFMediaType> min = WindowsAudioUtils::formatToMediaType(*m_wmf, fin);
    ComPtr<IMFMediaType> mout = WindowsAudioUtils::formatToMediaType(*m_wmf, fout);

    HRESULT hr = m_resampler->SetInputType(m_inputStreamID, min.Get(), 0);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcAudioResampler, "Failed to set input type {}", QSystemError::windowsComString(hr));
        return false;
    }

    hr = m_resampler->SetOutputType(0, mout.Get(), 0);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcAudioResampler, "Failed to set output type {}", QSystemError::windowsComString(hr));
        return false;
    }

    MFT_OUTPUT_STREAM_INFO streamInfo;
    hr = m_resampler->GetOutputStreamInfo(0, &streamInfo);
    if (FAILED(hr)) {
        qz::Log::cat_warn(qLcAudioResampler, "Could not obtain stream info {}", QSystemError::windowsComString(hr));
        return false;
    }

    m_inputFormat = fin;
    m_outputFormat = fout;

    return true;
}

void WindowsResampler::setStartTimeOffset(std::chrono::microseconds startTime)
{
    m_startTimeOffset = startTime;
}

